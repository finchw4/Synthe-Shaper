#include <F28x_Project.h>
#include <stdlib.h>

void initSPI(){

    // NOTE: NOTE USING FIFO MODE

    EALLOW;
    // disable pull ups
    GpioCtrlRegs.GPBPUD.bit.GPIO63 = 1; // MOSI pull up
    GpioCtrlRegs.GPCPUD.bit.GPIO64 = 0; // MISO pull up
    GpioCtrlRegs.GPCPUD.bit.GPIO65 = 1; // CLK pull up
    GpioCtrlRegs.GPCPUD.bit.GPIO66 = 1; // /CS pull up

    // set directions
    GpioCtrlRegs.GPBDIR.bit.GPIO63 = 1;
    GpioCtrlRegs.GPCDIR.bit.GPIO64 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO65 = 1;
    GpioCtrlRegs.GPCDIR.bit.GPIO66 = 1;

    // set pin functions
    GpioCtrlRegs.GPBGMUX2.bit.GPIO63    = 3; // select MOSI
    GpioCtrlRegs.GPBMUX2.bit.GPIO63     = 3;
    GpioCtrlRegs.GPCGMUX1.bit.GPIO64    = 3; // select MISO
    GpioCtrlRegs.GPCMUX1.bit.GPIO64     = 3;
    GpioCtrlRegs.GPCGMUX1.bit.GPIO65    = 3; // select CLK
    GpioCtrlRegs.GPCMUX1.bit.GPIO65     = 3;
    GpioCtrlRegs.GPCGMUX1.bit.GPIO66    = 3; // select /CS
    GpioCtrlRegs.GPCMUX1.bit.GPIO66     = 3;

    // configure SPI registers
    SpibRegs.SPICCR.bit.SPISWRESET  = 0;    // clear the bit at the beginning of configuration
    SpibRegs.SPICCR.bit.CLKPOLARITY = 1;
    SpibRegs.SPICCR.bit.HS_MODE     = 0;
    SpibRegs.SPICCR.bit.SPILBK      = 0;
    SpibRegs.SPICCR.bit.SPICHAR     = 7;    // 8 bit transmissions
    SpibRegs.SPICCR.bit.SPISWRESET  = 1;    // set the bit at the end of configuration
    SpibRegs.SPICTL.all             = 0x6;  // set clock phase, master, and enable talk
    SpibRegs.SPIBRR.bit.SPI_BIT_RATE= 0xFF;  // LSPCLK/7 = 50MHz/7 = 7.14 MHz (try changing to FF if needed)
    SpibRegs.SPIPRI.bit.FREE        = 1;
}

void sendSPI(Uint16 data){

    //data = data & 0xFF; //mask off any data beyond one byte
    SpibRegs.SPIDAT = data << 8; //left justify data when writing
    while(!SpibRegs.SPISTS.bit.INT_FLAG);
    Uint16 temp = SpibRegs.SPIRXBUF;
}

void sendDummyBytes(int numBytes){
    for(int i = 0; i < numBytes; i++){
        sendSPI(0xFF);
    }
}

Uint16 sendCommand(Uint16 cmd_num, Uint32 arg, Uint16 crc){

    Uint16 rsp = 0xFF;
    while(!GpioDataRegs.GPCDAT.bit.GPIO64); // wait until DO is high to send command
    sendSPI(cmd_num + 64);
    sendSPI((arg >> 24) & 0xFF);
    sendSPI((arg >> 16) & 0xFF);
    sendSPI((arg >> 8) & 0xFF);
    sendSPI(arg & 0xFF);
    if(crc != 0){
        sendSPI(crc);
    }
    rsp = SpibRegs.SPIRXBUF;
    while(rsp == 0xFF){     // this will be a problem if SD card is actually sending out 0xFF
        sendDummyBytes(1);
        rsp = SpibRegs.SPIRXBUF;
    }
    sendDummyBytes(1);
    return rsp;
}

/******************************************************************/
                          /*INIT SD*/
/******************************************************************/

void initSD(){

    initSPI();

    sendDummyBytes(10); // send dummy bytes so SD card can figure out the clock

    Uint32 response;

    // send CMD0 and wait for 0x01 response
    response = sendCommand(0,0,0x95);

    // send CMD8 and hope to get response with matching args
    response = sendCommand(8,0x1AA,0x87);

    // get 4 byte response, shifting to be able to hold entire response at once
    response = SpibRegs.SPIRXBUF;
    sendDummyBytes(1);
    response = response << 8 | SpibRegs.SPIRXBUF;
    sendDummyBytes(1);
    response = response << 8 | SpibRegs.SPIRXBUF;
    sendDummyBytes(1);
    response = response << 8 | SpibRegs.SPIRXBUF;
    sendDummyBytes(1);

    // send ACMD41
    while(response != 0x00){
        response = sendCommand(55,0,0x65);    // every ACMD starts with a CMD55
        response = sendCommand(41,0x40000000,0x77); // I don't think the CRC matters anymore
    }

    // send CMD58 and make sure that bit 30 of the response is set, indicating block addressing
    response = sendCommand(58,0,0);
    response = SpibRegs.SPIRXBUF;
    sendDummyBytes(1);
    response = response << 8 | SpibRegs.SPIRXBUF;
    sendDummyBytes(1);
    response = response << 8 | SpibRegs.SPIRXBUF;
    sendDummyBytes(1);
    response = response << 8 | SpibRegs.SPIRXBUF;
    sendDummyBytes(1);

    EALLOW;
    SpibRegs.SPIBRR.bit.SPI_BIT_RATE= 0x06; // LSPCLK/7 = 50MHz/7 = 7.14 MHz
    sendDummyBytes(100);

}

/******************************************************************/
                          /*WRITE BLOCK*/
/******************************************************************/

void writeBlock(Uint32 addr, Uint16* block){

    Uint32 response = 0;
    response = response + 1; // I just added this to get rid of the annoying warning

    response = sendCommand(24,addr,0xAA);
    sendDummyBytes(2);

    //while(!GpioDataRegs.GPCDAT.bit.GPIO64); // DO
    sendSPI(0xFE);
    for(int i = 0; i < 512; i++){
        sendSPI(block[i]);
    }
    sendSPI(0xAA);  // dummy crc values
    sendSPI(0xAA);

    sendDummyBytes(1);
    response = SpibRegs.SPIRXBUF;

    sendDummyBytes(32000); // There's gotta be a more efficient way to do this
}

/******************************************************************/
                         /*READ BLOCK*/
/******************************************************************/

Uint16 * readBlock(Uint32 addr){

    Uint32 response;
    static Uint16 block[512];

    // send CMD17: read single block
    response = sendCommand(17,addr,0xAA); // another dummy crc

    // wait for data token
    while(response != 0xFE){    // need to add error handling here for bad reads
        sendDummyBytes(1);
        response = SpibRegs.SPIRXBUF;
    }

    for(int i = 0; i < 512; i++){
        sendDummyBytes(1);
        block[i] = SpibRegs.SPIRXBUF;
    }

    sendDummyBytes(32000); // idk why this is necessary
    return block;
}
