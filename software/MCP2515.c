#include "MCP2515.h"

void MCP2515_init()
{
	//DDRC |= (1<<MCP2515_PIN_RESET);		//set the reset pin to output

	MCP2515_PORT_RESET &= ~(1<<MCP2515_PIN_RESET);
	_delay_us(50);
	MCP2515_PORT_RESET |= (1<<MCP2515_PIN_RESET);


	//SPI_send_byte(MCP2515_RESET); //instead of hard reset, perform software rest.
	MCP2515_bit_modify(MCP2515_CANCTRL,128,0xE0);		//put the device into configuration mode.

	MCP2515_reg_write(MCP2515_CNF1, 0x04);	//SJW = 0(1),BRP = 4(5)--> number in brackets is actual value, as mcp2515 adds 1.
	MCP2515_reg_write(MCP2515_CNF2, 0xCA);	//BTL = 1, SAM = 1, PHSEG1 = 001(2), PRSEG = 010 (3)
	MCP2515_reg_write(MCP2515_CNF3, 0x01);	//SOF = 0, WAKFIL = 0, PHSEG2 = 001(2).
	MCP2515_reg_write(MCP2515_CANINTE, 0b00000011);	//enable interrupt in rx0, rx1, tx0, tx1, tx2.
	MCP2515_reg_write(MCP2515_RTSCTRL, 0x01); //probably want to move this to a tx init function. eventually. if it aint broke don't fix it...
	//MCP2515_init_Rx();
	MCP2515_bit_modify(MCP2515_CANCTRL, 0x00, 0xE0);		//put the device into it's functional mode currently: normal 0xE0, listen is 0x60
	
}


void MCP2515_reg_write(uint8_t reg_address, uint8_t reg_value)
{
	MCP2515_PORT_CS &= ~(1<<MCP2515_PIN_CS);			//unset CS so MCP2515 knows we are talking
	SPI_send_byte(MCP2515_WRITE);
	SPI_send_byte(reg_address);
	SPI_send_byte(reg_value);
	MCP2515_PORT_CS |= (1<<MCP2515_PIN_CS);			//set the CS.
	//if(MCP2515_reg_read(reg_address) == reg_value)flash_LED(1,RED_LED);
}

void MCP2515_instruction(uint8_t instruction)
{
	MCP2515_PORT_CS &= ~(1<<MCP2515_PIN_CS);			//unset CS so MCP2515 knows we are talking
	SPI_send_byte(instruction);
	MCP2515_PORT_CS |= (1<<MCP2515_PIN_CS);			//set the CS.
}

void MCP2515_tx(uint8_t identifier, uint8_t transmit_buffer, uint8_t device_id, uint8_t recipient, uint8_t type_code, uint8_t dlc, uint8_t * data)
{
	if(transmit_buffer == 0)return;
	MCP2515_reg_write(transmit_buffer, 0x03);			//we shall set this to the highest priority so that it sends it immediately.
	identifier = identifier & 3;
	MCP2515_reg_write(transmit_buffer+1, (identifier<<6)|(DEVICE_ID>>3));	//AMU identifier bit and the AMU device ID

	MCP2515_reg_write(transmit_buffer+2, (1<<3)|(DEVICE_ID<<5)|(recipient>>8));

	MCP2515_reg_write(transmit_buffer+3, recipient);			//works
	MCP2515_reg_write(transmit_buffer+4, type_code);
	MCP2515_reg_write(transmit_buffer+5, dlc);
	for (uint8_t byteCount = 0; byteCount < dlc; byteCount++)
	{
		MCP2515_reg_write(transmit_buffer+6+byteCount, *(data+byteCount));
		//uint8_t status = MCP2515_reg_read(transmit_buffer+6+byteCount);
		//if(status == *(data+byteCount))flash_LED(1,RED_LED);
	}
	MCP2515_instruction(128|(1<<((transmit_buffer>>4) - 3)));		//creates an instruction that matches the mob that is chosen.
	//tx0 is 3<<4, tx1 is 4<<4, tx2 is 5<<4
	//PORTC &= ~(1<<PINC7);
	//_delay_us(50);
	//if((MCP2515_receive_status() & 4)==4)flash_LED(1,RED_LED);			//tx request bit set.
	//PORTC |= (1<<PINC7);

	//flash_LED(1, RED_LED);
}

void MCP2515_bit_modify(uint8_t reg_address, uint8_t reg_value, uint8_t reg_mask)
{
	MCP2515_PORT_CS &= ~(1<<MCP2515_PIN_CS);			//unset CS so MCP2515 knows we are talking
	SPI_send_byte(MCP2515_BITMODIFY);		//send instruction of bitmodify
	SPI_send_byte(reg_address);				//send address
	SPI_send_byte(reg_mask);				//send the mask
	SPI_send_byte(reg_value);				//send the data
	MCP2515_PORT_CS |= (1<<MCP2515_PIN_CS);			//set the CS.
}

uint8_t MCP2515_receive_status()
{
	uint8_t mcp2515_status[2];
	MCP2515_PORT_CS &= ~(1<<MCP2515_PIN_CS);			//unset CS so MCP2515 knows we are talking
	SPI_send_byte(0xA0);					//send retrieve status instruction
	mcp2515_status[0] = SPI_send_byte(0x00);//send don't care bits while mcp2515 is retrieving data.
	mcp2515_status[1] = SPI_send_byte(0x00);//duplicate data is retrieved again. nothing to do with this second lot yet.
	MCP2515_PORT_CS |= (1<<MCP2515_PIN_CS);			//set the CS.
	return mcp2515_status[0];					//send it back for analysis.
}

/* README before using this!!!!!!!!
 * this function takes a pointer to an array of data to write to, it must be either 8 or 13 elements in size for safe use.
 * 13 elements in size for an address of RXBnSIDH
 * 8  elements in size for an address of RXBnD0
 * e.g-------------------------
 *
 * uint8_t data[13];
 * MCP2515_RxBufferRead(data, RXB0SIDH);
 *
 * ---------------------------> this example will fill the data[13] array with elements from RXB0SIDH-->RXB0D7
 *
 * uint8_t data[8];
 * MCP2515_RxBufferRead(data, RXB0D0);
 * ---------------------------> this example will fill the data[8] array with bytes from RXB0D0-->RXB0D7
 *
 * This function also automatically clears the interrupt flag CANINTF.RX0IF(in this case)
 */
void MCP2515_RxBufferRead(uint8_t * data, uint8_t startingAddress)
{

	//the following line combines the instruction(0b10010000), with: 0b100 for rxb0 or 0b000 for rxb1, and: 0b10 for data starting at data0, or 0b00 for SIDH
	uint8_t instruction = 0b10010000|((startingAddress > 0x70)<<2)|((startingAddress == MCP2515_RXB0D0 || startingAddress == MCP2515_RXB1D0)<<1);
	MCP2515_PORT_CS &= ~(1<<MCP2515_PIN_CS);			//lower CS.
	SPI_send_byte(instruction);							//send instruction for stream of data
	//loop counts to 8 or 12 depending on whether bit 1 of instruction is set.
	for(uint8_t counter = 0; counter < (8 + 4*((instruction & 2)==0)); counter++)
	{
		*data = SPI_send_byte(0x00);
		data++;
	}
	MCP2515_PORT_CS |= (1<<MCP2515_PIN_CS);				//raise CS.
}

uint8_t MCP2515_reg_read(uint8_t reg_address)
{
	uint8_t read_result;
	MCP2515_PORT_CS &= ~(1<<MCP2515_PIN_CS);			//unset CS so MCP2515 knows we are talking
	SPI_send_byte(0x03);
	SPI_send_byte(reg_address);
	read_result = SPI_send_byte(0x00);
	MCP2515_PORT_CS |= (1<<MCP2515_PIN_CS);			//set the CS.
	return read_result;
}

uint8_t MCP2515_findFreeTxBuffer()
{
	//uint8_t MCP2515_TxBuffer = 0;
	//flash_LED(1,RED_LED);
	//MCP2515_TxBuffer = (MCP2515_reg_read(MCP2515_CANINTF)& 0b00011100);			//get interrupt status, only the txbuffer empty ones though
	if		((MCP2515_reg_read(MCP2515_TX0) & 0b00001000)== 0)						//if tx0 is free,
	{
		return MCP2515_TX0;
	}
	else if	((MCP2515_reg_read(MCP2515_TX1) & 0b00001000)== 0)						//if tx1 is free,
	{
		return MCP2515_TX1;
	}
	else if	((MCP2515_reg_read(MCP2515_TX2) & 0b00001000)== 0)						//if tx2 is free,
	{
		return MCP2515_TX2;
	}
	else {return 0x00;	}														//otherwise none are free.
}

void MCP2515_FilterInit(uint8_t filterNum, uint32_t filterID)
{
	switch(filterNum)
	{
		case 0:
			MCP2515_reg_write(MCP2515_RXF0SIDH, (filterID>>21) & 0xFF);
			MCP2515_reg_write(MCP2515_RXF0SIDL, (((filterID>>13) & 224)| ((filterID>>16) & 3) | ((1<<3) & 0xFF)));
			MCP2515_reg_write(MCP2515_RXF0EID8, (filterID>>8) & 0xFF );
			MCP2515_reg_write(MCP2515_RXF0EID0, filterID & 0xFF );
		case 1:
			MCP2515_reg_write(MCP2515_RXF1SIDH, (filterID>>21) & 0xFF);
			MCP2515_reg_write(MCP2515_RXF1SIDL, (((filterID>>13) & 224)| ((filterID>>16) & 3) | ((1<<3) & 0xFF)));
			MCP2515_reg_write(MCP2515_RXF1EID8, (filterID>>8) & 0xFF );
			MCP2515_reg_write(MCP2515_RXF1EID0, filterID & 0xFF );
		case 2:
			MCP2515_reg_write(MCP2515_RXF2SIDH, (filterID>>21) & 0xFF);
			MCP2515_reg_write(MCP2515_RXF2SIDL, (((filterID>>13) & 224)| ((filterID>>16) & 3) | ((1<<3) & 0xFF)));
			MCP2515_reg_write(MCP2515_RXF2EID8, (filterID>>8) & 0xFF );
			MCP2515_reg_write(MCP2515_RXF2EID0, filterID & 0xFF );
		case 3:
			MCP2515_reg_write(MCP2515_RXF3SIDH, (filterID>>21) & 0xFF);
			MCP2515_reg_write(MCP2515_RXF3SIDL, (((filterID>>13) & 224)| ((filterID>>16) & 3) | ((1<<3) & 0xFF)));
			MCP2515_reg_write(MCP2515_RXF3EID8, (filterID>>8) & 0xFF );
			MCP2515_reg_write(MCP2515_RXF3EID0, filterID & 0xFF );
		case 4:
			MCP2515_reg_write(MCP2515_RXF4SIDH, (filterID>>21) & 0xFF);
			MCP2515_reg_write(MCP2515_RXF4SIDL, (((filterID>>13) & 224)| ((filterID>>16) & 3) | ((1<<3) & 0xFF)));
			MCP2515_reg_write(MCP2515_RXF4EID8, (filterID>>8) & 0xFF );
			MCP2515_reg_write(MCP2515_RXF4EID0, filterID & 0xFF );
		case 5:
			MCP2515_reg_write(MCP2515_RXF5SIDH, (filterID>>21) & 0xFF);
			MCP2515_reg_write(MCP2515_RXF5SIDL, (((filterID>>13) & 224)| ((filterID>>16) & 3) | ((1<<3) & 0xFF)));
			MCP2515_reg_write(MCP2515_RXF5EID8, (filterID>>8) & 0xFF );
			MCP2515_reg_write(MCP2515_RXF5EID0, filterID & 0xFF );
			
	}
		
}

uint8_t MCP2515_RXInit(int8_t mob, uint32_t IDmsk)	//write IDmsk 0 if you do not wish to use a mask, and this MOB will receive all messages
{
	if (mob > 1)return 0;
	switch(mob)
	{
		case 0:
			MCP2515_reg_write(MCP2515_RXM0SIDH, (IDmsk>>21) & 0xFF);	//shift the 32 bit mask into the respective registers
			MCP2515_reg_write(MCP2515_RXM0SIDL, (((IDmsk>>13) & 224)| ((IDmsk>>16) & 3)));
			MCP2515_reg_write(MCP2515_RXM0EID8, (IDmsk>>8) & 0xFF );
			MCP2515_reg_write(MCP2515_RXM0EID0, IDmsk & 0xFF );
			if(IDmsk) MCP2515_reg_write(MCP2515_RXB0CTRL, 0b00000100);	//Enable reception using masks and filters. Enable BUKT bit, which enables RXB1 to be used if RXB0 is full
			else MCP2515_reg_write(MCP2515_RXB0CTRL, 0b01100100);		//Enable reception without masks and filters. 
		case 1:
			MCP2515_reg_write(MCP2515_RXM1SIDH, (IDmsk>>21) & 0xFF);	//shift the 32 bit mask into the respective registers
			MCP2515_reg_write(MCP2515_RXM1SIDL, (((IDmsk>>13) & 224)| ((IDmsk>>16) & 3)));
			MCP2515_reg_write(MCP2515_RXM1EID8, (IDmsk>>8) & 0xFF );
			MCP2515_reg_write(MCP2515_RXM1EID0, IDmsk & 0xFF );
			if(IDmsk) MCP2515_reg_write(MCP2515_RXB1CTRL, 0b00000000);	//Enable reception using masks and filters. 
			else MCP2515_reg_write(MCP2515_RXB1CTRL, 0b01100000);		//Enable reception without masks and filters.
	}	
	return 1;
}

void MCP2515_TX(int8_t mob, uint8_t numBytes, uint8_t * data, uint32_t ID)
{
	if(mob == 0)return;						//no free mob or invalid mob
	MCP2515_reg_write(mob, 0x03);			//we shall set this to the highest priority so that it sends it immediately.
	MCP2515_reg_write(mob+1, (ID>>21) & 0xFF);	//shift the ID data to fill the respective MCP registers
	MCP2515_reg_write(mob+2, (((ID>>13) & 224)| ((ID>>16) & 3) | ((1<<3) & 0xFF)));	//set the EXIDE bit, which makes it extended (CAN 2.0B)
	MCP2515_reg_write(mob+3, (ID>>8) & 0xFF );
	MCP2515_reg_write(mob+4, ID & 0xFF );
	MCP2515_reg_write(mob+5, numBytes);		//set how many bytes we wish to send
	//MCP2515_reg_write(mob, 0x03);
	//MCP2515_reg_write(mob+1, 0);
	//MCP2515_reg_write(mob+2, (1<<3));
	//MCP2515_reg_write(mob+3, 0);
	//MCP2515_reg_write(mob+4, 0);
	//MCP2515_reg_write(mob+5, 0);
	
	for (uint8_t byteCount = 0; byteCount < numBytes; byteCount++)
	{
		MCP2515_reg_write(mob+6+byteCount, *(data+byteCount));	//fill the data bytes register.
	}
	MCP2515_instruction(128|(1<<((mob>>4) - 3)));		//creates an instruction that matches the mob that is chosen.
	//PORTC &= ~(1<<PINC7);		//Drop the TX
	//_delay_us(50);
	//PORTC |= (1<<PINC7);		
}


uint8_t MCP2515_check_receive_status()
{
	uint8_t status;
	MCP2515_PORT_CS &= ~(1<<MCP2515_PIN_CS);			//unset CS so MCP2515 knows we are talking
	SPI_send_byte(MCP2515_RXSTATUS);
	status = SPI_send_byte(0x00);						//send zeros to get data
	SPI_send_byte(0x00);							//MCP2515 will repeat the data output, so send another batch of zeros
	MCP2515_PORT_CS |= (1<<MCP2515_PIN_CS);			//set the CS
	return status;
}

uint8_t MCP2515_send_audit_request()
{
	//uint8_t data = 0x00;
	uint8_t free_buffer = MCP2515_findFreeTxBuffer();						//obtain a free transmit buffer.

	if(free_buffer)
	{
		//flash_LED(1, YELLOW_LED, 50);
		//MCP2515_tx(AMU,free_buffer,DEVICE_ID,0x00,AUDIT_REQUEST,0,&data);		//send a CAN packet from a free buffer, with recipient of our CMU ID, type audit request (to now get others to talk), with 1 byte of zeros
		//MCP2515_TX(MCP2515_findFreeTxBuffer(), 0, &status, ((uint32_t)1<<27)|((uint32_t)DEVICE_ID<<18)|AUDIT_REQUEST );
		return 1;					//return successful
	}
	else
	{
		return 0;					//return error, there were no free buffers.
	}
}