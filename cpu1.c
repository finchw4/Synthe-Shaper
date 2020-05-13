/*
 * main1.c
 *
 *  Created on: Feb 7, 2020
 *      Author: Nicolas Bontems
 */
#include "driverlib.h"
#include "device.h"
#include "F28x_Project.h"
#include <adc.h>
#include "SD.h"
#include "MIDI.h"
/*************************************************************************************************************/
/*************************Put in a linked header file*********************************************************/
#define numEffects 2
#define flag1to2 (1UL << 4)
#define flag2to1 (1UL << 7)
void sdelay();
void ldelay();
void GPIO();
void writeCommand(Uint16 command);
void writeData(Uint16 data);
void PowerUp(); //this will pull reset signal low and turn disp on and configure everything
void FillUpBlock(Uint16 x, Uint16 y, Uint32 width, Uint32 height, Uint16 c1); //then location on screen and also color of the block
void writeLetter(Uint16 x, Uint16 y, Uint16 size, Uint16 address, Uint16 color1, Uint16 color2); //position x and y, size of the letter, which letter, and then colors
void adcInitialize();
void readx();
void ready();
void rotarycheck();
void secondary1();
void secondary2();
void secondary3();
void secondary4();
void transfercalculation(Uint16 number);
void startmenu();
void menuInit();
void store();
void save();
void draw(Uint16 on);//on=1 if you want to draw, else put on=0 to just read the screen input
void crossoutcheck();
int16 booltemp=0;
int16 rotarystate=0;
int16 rotarystate2=0;
int16 rotarystate3=0;
volatile int16 buffer0[256];
volatile int16 buffer1[256];
Uint32 animation=0;
Uint16 animationtemp=0;
Uint16 menu=1;
Uint16 divider=30;
int16 xt=0;
int16 bootuptime=0;
Uint16 clearuptime=0;
int16 yt=0;
Uint16 temp2;
Uint16 command;
Uint16 data;
Uint16 HH=0x03;
Uint16 HL=0x21;
Uint16 VH=0x01;
Uint16 VL=0xE1;
Uint16 state=0;
int16 local1=0;
int16 local2=0;
int16 touchcolor=0;
Uint32 counter=1160000;
int16 temp=-1;
int16 temp3=0;
int16 temp4=-1;
int16 temp5;
Uint16 speed=400; //for the rotary encoder
//int16 sustain=-1;
//int16 min=400; //deault min frequency for BPF
//int16 max=1500; //deault max frequency for BPF
int16 minmaxflag=0;

int16 num1=0; //deault min frequency for BPF
int16 num2=0; //deault min frequency for BPF
int16 num3=4; //deault min frequency for BPF
int16 num4=0; //deault min frequency for BPF

//int16 osctemp=0;
//int16 unitemp=0;
int16 value=0;
int16 value2=0;
int16 folder=0;
int16 cancel=0;

Uint16 * rBlock;

typedef enum{
    leftVolume,
    rightVolume
}effectNames_t;
/**************************************************************************************************************/
typedef struct SETUP
{
    //arpeggio
    bool arpTrue; //if true then have arp
    int16 oct;
    Uint16 freq;
    float arp[23];
    Uint32 arpspeed;

   //Volume
    int16 sustain; //sustain position
   Uint32 vol[23];
   Uint32 volspeed;

   //Unison
   int16 unitemp; //nb of voices
   float  uni[23];
   Uint32 unispeed;

   //Filter
   bool filTrue; //if true then have fil
   int16 min;
   int16 max;
   int16  fil[23];
   Uint32 filspeed;
}csetup1;  //current setup and folders


csetup1 fol[4];
csetup1 csetup;
//shared ram stuff


#pragma DATA_SECTION(csetupSEND, "SHARERAMGS0")
volatile csetup1 csetupSEND[0];
//effectStatus_t effects[numEffects];

void main(void){
    //
    // Initialize device clock and peripherals
    //
    Uint16 prevCount = 0;
    Device_init();

    //giving GS0 to CPU1
    EALLOW;
     MemCfgRegs.GSxMSEL.bit.MSEL_GS0=0;
     EDIS;

    //
    // Disable pin locks and enable internal pullups.
    //
    Device_initGPIO();
    GPIO();
    //get codec going
    initMcbspBCPU2();

    InitSPIA(); //initialize SPIA (DSP as a master)
    InitAIC23(); //initialize the codec

    //MY stuff Nick
    Uint32 c=0;
    Uint32 v=0x800000;
    InitSysCtrl();
    initUART();
    PowerUp();
    initSD();
    FillUpBlock(0,0,800,480,0b11111111);
    adcInitialize();
   csetup.min=450;
   csetup.max=900;
   csetup.oct=1;
   csetup.freq=8;
   //struct folders folder[3]; //4 folders (?)
  // struct currentsetup csetup;
   //intra-processor coms init
   InitIpc();
   //CPUvalue[0]=4;
   while(1) {
         if(menu==1){
         startmenu();
         }

         if(menu==0){
      draw(1);

      //***************************SEND STUFF TO CPU2*********************************
      for(int i = 0; i < 23; i++){
          csetupSEND[0].arp[i] = csetup.arp[i]/400;
      }
      csetupSEND[0].oct= csetup.oct;
      csetupSEND[0].freq= 24000/csetup.freq;
      csetupSEND[0].arpspeed= (21-(csetup.arpspeed+12))*388+209;
      csetupSEND[0].arpTrue= csetup.arpTrue;

      for(int i = 0; i < 23; i++){
          csetupSEND[0].vol[i] = csetup.vol[i] *125000;
      }
     // csetupSEND[0].osctemp = csetup.osctemp;
      csetupSEND[0].sustain= csetup.sustain;
      csetupSEND[0].volspeed= (21-(csetup.volspeed+12))*388+209;
      csetupSEND[0].unitemp= csetup.unitemp+1;
      for(int i = 0; i < 23; i++){
          csetupSEND[0].uni[i] = csetup.uni[i]/1200;
      }
      csetupSEND[0].unispeed = (21-(csetup.unispeed+12))*388+209;
      csetupSEND[0].min = csetup.min;
      csetupSEND[0].max= csetup.max;
      for(int i = 0; i < 23; i++){
          csetupSEND[0].fil[i] = ((csetup.max-csetup.min)/400)*csetup.fil[i]+csetup.min;;
      }
      csetupSEND[0].filspeed= (21-(csetup.filspeed+12))*388+209;
      csetupSEND[0].filTrue=csetup.filTrue;
      //**********************************************************************************

      //**************Rotary3 speed module*****************
      if(rotarystate3<-12 || rotarystate3 > 9)
      {rotarystate3=temp3;
      FillUpBlock(760+3*temp3,320,10,40,0b11111111);}
      if(temp3!=rotarystate3){
      FillUpBlock(760+3*temp3,320,10,40,0b11111111);
      FillUpBlock(760+3*rotarystate3,320,5,40,0);
      if(rotarystate==0){csetup.arpspeed=rotarystate3;}
      else if(rotarystate==1){csetup.volspeed=rotarystate3;}
      else if(rotarystate==2){csetup.unispeed=rotarystate3;}
      else if(rotarystate==3){csetup.filspeed=rotarystate3;}
      temp3=rotarystate3;
      }

      rotarycheck();

      //***************Rotary1 Module1**************
         if(rotarystate==0){
             if(temp!=0){
                 secondary1();
             temp=0;}
         }
         else  if(rotarystate==1){
             if(temp!=1){
                 secondary2();
                        temp=1;}
                    }
         else  if(rotarystate==2){
             if(temp!=2){
                 secondary3();
                                  temp=2;}
                              }
         else  if(rotarystate==3){
             if(temp!=3){
                 secondary4();
             temp=3;}
         }
         else  {rotarystate=temp;}

         //******************Rotary2 module2***********************
         //if unison is selected
         if(rotarystate==2){
         if(rotarystate2==0){
             if(temp4!=0){
                 FillUpBlock(700,135,15,120,0);
                 FillUpBlock(700,138,20,6,0b11111111);
                 csetup.unitemp=0;
             temp4=0;}
         }
         else  if(rotarystate2==1){
             if(temp4!=1){
                 FillUpBlock(700,135,15,120,0);
                 FillUpBlock(700,162,20,6,0b11111111);
                 csetup.unitemp=1;
                        temp4=1 ;}
                    }
         else  if(rotarystate2==2){
             if(temp4!=2){
                 FillUpBlock(700,135,15,120,0);
                 FillUpBlock(700,186,20,6,0b11111111);
                 csetup.unitemp=2;
                         temp4=2;}
                              }
         else{rotarystate2=temp4;}
         }
         //if Volume envelope is selected
         if(rotarystate==1){
             if(rotarystate2<0){rotarystate2=22;}
             else if(rotarystate2>22){rotarystate2=0;}

             if(rotarystate2!=csetup.sustain){
             transfercalculation(rotarystate2);
             writeLetter(720,210,20,30+num3,0b0,0b11111111);
             writeLetter(735,210,20,30+num4,0b0,0b11111111);
             FillUpBlock(30*csetup.sustain+12,400,5,15,0);
             FillUpBlock(30*rotarystate2+12,400,5,15,0b11111111);
             csetup.sustain=rotarystate2;}
         }

         //if Filter is selected
         if(rotarystate==3){
               //put boundaries
             if(rotarystate2<0){rotarystate2=0;}
             else if(rotarystate2>2000){rotarystate2=2000;}
             if(minmaxflag==0){
             if(rotarystate2!=csetup.min){

             transfercalculation(rotarystate2);
             writeLetter(720,160,20,30+num1,0b0,0b11111111);
             writeLetter(735,160,20,30+num2,0b0,0b11111111);
             writeLetter(750,160,20,30+num3,0b0,0b11111111);
             writeLetter(765,160,20,30+num4,0b0,0b11111111);
             csetup.min=rotarystate2;}}
             else if(minmaxflag==-1){
                 if(rotarystate2!=csetup.max){
               transfercalculation(rotarystate2);
               writeLetter(720,215,20,30+num1,0b0,0b11111111);
               writeLetter(735,215,20,30+num2,0b0,0b11111111);
               writeLetter(750,215,20,30+num3,0b0,0b11111111);
               writeLetter(765,215,20,30+num4,0b0,0b11111111);
               csetup.max=rotarystate2;}}
         }
         //if arp is selected
         if(rotarystate==0){
               //put boundaries

             if(minmaxflag==0){
                 if(rotarystate2<1){rotarystate2=1;}
                 else if(rotarystate2>5){rotarystate2=5;}
             if(rotarystate2!=csetup.oct){
             transfercalculation(rotarystate2);
             writeLetter(720,160,20,30+num1,0b0,0b11111111);
             writeLetter(735,160,20,30+num2,0b0,0b11111111);
             writeLetter(750,160,20,30+num3,0b0,0b11111111);
             writeLetter(765,160,20,30+num4,0b0,0b11111111);
             csetup.oct=rotarystate2;}}
             else if(minmaxflag==-1){
                 if(rotarystate2<1){rotarystate2=1;}
                 else if(rotarystate2>20){rotarystate2=20;}
                 if(rotarystate2!=csetup.freq){
               transfercalculation(rotarystate2);
               writeLetter(720,215,20,30+num1,0b0,0b11111111);
               writeLetter(735,215,20,30+num2,0b0,0b11111111);
               writeLetter(750,215,20,30+num3,0b0,0b11111111);
               writeLetter(765,215,20,30+num4,0b0,0b11111111);
               csetup.freq=rotarystate2;}}
         }
         //*********************************************************************************************

         //*********************Button managment****************************
         //for arp and fil selections
         if(GpioDataRegs.GPADAT.bit.GPIO29==0){
             DELAY_US(1000);//debounce
             while(GpioDataRegs.GPADAT.bit.GPIO29==0){}
             minmaxflag=~minmaxflag;
             if(minmaxflag==0){
                 if(rotarystate==3){rotarystate2=csetup.min;}
                 else if(rotarystate==0){rotarystate2=csetup.oct;}
             FillUpBlock(700,135,15,100,0);
             FillUpBlock(700,138,20,6,0b11111111);}
             else if(minmaxflag==-1){
                 if(rotarystate==3){rotarystate2=csetup.max;}
                 else if(rotarystate==0){rotarystate2=csetup.freq;}
             FillUpBlock(700,135,15,100,0);
             FillUpBlock(700,193,20,6,0b11111111);}
             DELAY_US(1000);}//debounce
         //**********for folder selection*********
          if(GpioDataRegs.GPADAT.bit.GPIO10==0){
             DELAY_US(1000);//debounce
             while(GpioDataRegs.GPADAT.bit.GPIO10==0){}
             csetup=fol[0];
             csetup.sustain=fol[0].sustain;
             crossoutcheck();
              writeLetter(10,430,20,6,0b0,0b11111111);
              writeLetter(25,430,20,15,0b0,0b11111111);
              writeLetter(40,430,20,12,0b0,0b11111111);
              writeLetter(55,430,20,31,0b0,0b11111111);
             if(rotarystate==0){secondary1();}
             else if(rotarystate==1){secondary2();}
             else if(rotarystate==2){secondary3();}
             else if(rotarystate==3){secondary4();}
             DELAY_US(1000);}//debounce

          if(GpioDataRegs.GPADAT.bit.GPIO11==0){
             DELAY_US(1000);//debounce
             while(GpioDataRegs.GPADAT.bit.GPIO11==0){}
             csetup=fol[1];
             csetup.sustain=fol[1].sustain;
             crossoutcheck();
             writeLetter(10,430,20,6,0b0,0b11111111);
             writeLetter(25,430,20,15,0b0,0b11111111);
             writeLetter(40,430,20,12,0b0,0b11111111);
             writeLetter(55,430,20,32,0b0,0b11111111);
             if(rotarystate==0){secondary1();}
             else if(rotarystate==1){secondary2();}
             else if(rotarystate==2){secondary3();}
             else if(rotarystate==3){secondary4();}
             DELAY_US(1000);}//debounce


          if(GpioDataRegs.GPADAT.bit.GPIO14==0){
             DELAY_US(1000);//debounce
             while(GpioDataRegs.GPADAT.bit.GPIO14==0){}
             csetup=fol[2];
             csetup.sustain=fol[2].sustain;
             crossoutcheck();
             writeLetter(10,430,20,6,0b0,0b11111111);
             writeLetter(25,430,20,15,0b0,0b11111111);
             writeLetter(40,430,20,12,0b0,0b11111111);
             writeLetter(55,430,20,33,0b0,0b11111111);
             if(rotarystate==0){secondary1();}
             else if(rotarystate==1){secondary2();}
             else if(rotarystate==2){secondary3();}
             else if(rotarystate==3){secondary4();}
             DELAY_US(1000);}//debounce


          if(GpioDataRegs.GPADAT.bit.GPIO15==0){
             DELAY_US(1000);//debounce
             while(GpioDataRegs.GPADAT.bit.GPIO15==0){}
             csetup=fol[3];
             csetup.sustain=fol[3].sustain;
             crossoutcheck();
             writeLetter(10,430,20,6,0b0,0b11111111);
             writeLetter(25,430,20,15,0b0,0b11111111);
             writeLetter(40,430,20,12,0b0,0b11111111);
             writeLetter(55,430,20,34,0b0,0b11111111);
             if(rotarystate==0){secondary1();}
             else if(rotarystate==1){secondary2();}
             else if(rotarystate==2){secondary3();}
             else if(rotarystate==3){secondary4();}
             DELAY_US(1000);}//debounce
//*******************for arp / fil true false
          if(GpioDataRegs.GPEDAT.bit.GPIO130==0){
             DELAY_US(1000);//debounce
             while(GpioDataRegs.GPEDAT.bit.GPIO130==0){}
             if(rotarystate==3){
             if(csetup.filTrue==false){csetup.filTrue=true;}
             else {csetup.filTrue=false;}}
             if(rotarystate==0){
             if(csetup.arpTrue==false){csetup.arpTrue=true;}
             else {csetup.arpTrue=false;}}

             crossoutcheck();

             DELAY_US(1000);}//debounce

         //exit button
         if(local1>690 & local1<760 & local2>460 & local2<500){menu=1;
         FillUpBlock(0,0,800,480,0b11111111);}

         //save button
         if(local1>690 & local1<760 & local2>405 & local2<430){menu=1;
         FillUpBlock(0,0,800,480,0b11111111);
         save();
         }

         //if(GpioDataRegs.GPBDAT.bit.GPIO41==1){
            // FillUpBlock(0,0,695,400,0b11111111);}

      }
     }
  }

  void PowerUp()
  {
      //Assert RESET signal low for at least 100us first to reset controller
      GpioDataRegs.GPBDAT.bit.GPIO32 = 0;
      for (Uint32 c = 0; c < 0xFFFFF; c++) {}
      GpioDataRegs.GPBDAT.bit.GPIO32 = 1;
      for (Uint32 c = 0; c < 0xFFFFF; c++) {}

      //Configure PLL
      writeCommand(0xE2); //setting PLL frequency (this is based on the application document)
      writeData(60);
      writeData(5);
      writeData(0x54);
      //Turn on PLL
      writeCommand(0xE0);
      writeData(0x01);
      //wait for at least 100us
      ldelay();
      ldelay();
      ldelay();
      ldelay();
      ldelay();
      ldelay();
      //read status bit (for troubleshooting)
      //Switch clock source to PLL
      writeCommand(0xE0);
      writeData(0x03);
      //software reset
      ldelay();
      writeCommand(0x01);
      ldelay();
      //configure dot clock frequency (Dot clock Freq = PLL Freq x (LCDC_FPR + 1) / 2^20)
      writeCommand(0xE6);
      writeData(0x04);
      writeData(0x70);
      writeData(0xA9);

      //********Configuring LCD panel ***************
      //setting 800*480 and other things
      writeCommand(0xB0); //setting lcd mode
      writeData(0x39); //24bit, disable FRC or dithering, TFT FRC, rising edge, LLINE and LFRAME active low
      writeData(0x00);
      writeData(HH); //Horizontal width =800-1=799=0x31F
      writeData(HL);
      writeData(VH); //vertical width= 480-1=0x1DF
      writeData(VL);
      writeData(0x00); //dummy for TFT

      //setting vertical period
      ldelay();
      ldelay();
      writeCommand(0xB6); //I have no idea what's going on here
      writeData(0x02); //VT
      writeData(0x0D); //VT
      writeData(0x00); // VPS
      writeData(0x23); //VPS
      writeData(0x02);  //VPW
      writeData(0x00);  //FPS
      writeData(0x00);  //FPS
      ldelay();
      ldelay();

      //setting horizontal period
      ldelay();
      ldelay();
      writeCommand(0xB4); //I have no idea what's going on here
      writeData(0x03); //HT
      writeData(0xA0); //HT
      writeData(0x00); //HPS
      writeData(0x88); //HPS
      writeData(0x2F); //HPW
      writeData(0x00); //LPS
      writeData(0x00); //LPS
      writeData(0x00);//LPSPP


      //*********************************************

      //setting backlight control PWM freq
      writeCommand(0xBE);
      writeData(0x08);  //PWM freq
      writeData(0x80);  //duty cycle
      writeData(0x01);  //something about DBC

      writeCommand(0x0B);
      writeData(0x00);
      writeCommand(0x0A);
      writeData(0x1C);

      //*****************Turning on the display****************(finally)
      writeCommand(0x29);

      //***************frame buffer stuff**************** (SC,EC,SP,EP)
      writeCommand(0x2A); //vertical buffer
      writeData(0x00); //SC
      writeData(0x00);
      writeData(HH); //EC (800-1=0x031F)
      writeData(HL);

      writeCommand(0x2B); //horizontal buffer
      writeData(0x00);  //SP
      writeData(0x00);
      writeData(VH); //EP ( 480-1=0x1DF)
      writeData(VL);


      //******optional rotate mode (See application note for SSD1963)************8
      writeCommand(0x36);
      writeData(0b00000000);
      //******Setup MCU 8-bit interface (?) **************
      writeCommand(0xF0); //pixel data interface
      writeData(0b000); //16bit packed (there's also 16-bit 565 format which im not sure what is different)
     //REVISED, lets make it 8bit actually
     //    Bit    7  6  5  4  3  2  1  0
     // Data   B   B  B  G  G  G  R  R
      //*******************************************************
      //this should be the end

  }

  void rotarycheck()
  {

      //rotary check
      if(GpioDataRegs.GPCDAT.bit.GPIO94==0){
          DELAY_US(speed);
   //CLockwise
          if(GpioDataRegs.GPCDAT.bit.GPIO95==1) {rotarystate++; }
          //counterclockwise
          else if(GpioDataRegs.GPCDAT.bit.GPIO95==0){rotarystate--;}

          while(GpioDataRegs.GPCDAT.bit.GPIO94==0){}//wait till clk back high
          DELAY_US(speed);
          }

      //rotary check
      if(GpioDataRegs.GPDDAT.bit.GPIO97==0){
          DELAY_US(speed);
   //CLockwise
          if(GpioDataRegs.GPADAT.bit.GPIO9==1) {rotarystate3++; }
          //counterclockwise
          else if(GpioDataRegs.GPADAT.bit.GPIO9==0){rotarystate3--;}

          while(GpioDataRegs.GPDDAT.bit.GPIO97==0){}//wait till clk back high
          DELAY_US(speed);
          }

      //rotary check
      if(GpioDataRegs.GPADAT.bit.GPIO16==0){
          DELAY_US(speed);
   //CLockwise
          if(GpioDataRegs.GPADAT.bit.GPIO8==1) {
              if(rotarystate==0 || rotarystate==1 || rotarystate==2){rotarystate2++; }
              else if(rotarystate==3){rotarystate2=rotarystate2+50;}
          }
          //counterclockwise
          else if(GpioDataRegs.GPADAT.bit.GPIO8==0){
          if(rotarystate==0 ||rotarystate==1 ||rotarystate==2 ){rotarystate2--; }
          else if(rotarystate==3){rotarystate2=rotarystate2-50;}}

          while(GpioDataRegs.GPADAT.bit.GPIO16==0){}//wait till clk back high
          DELAY_US(speed);
          }

  }

  void draw(Uint16 on)
  {

      //ADC ONE*******************************

  //        readx();
      for(long b=0;b<1000;b++){
             EALLOW;

       GpioDataRegs.GPDDAT.bit.GPIO111=1;
       GpioDataRegs.GPBDAT.bit.GPIO56=0;
       GpioDataRegs.GPBDAT.bit.GPIO52=0;
       GpioDataRegs.GPBDAT.bit.GPIO40=0;

       GpioCtrlRegs.GPDDIR.bit.GPIO111=1;
       GpioCtrlRegs.GPBDIR.bit.GPIO56=0;
       GpioCtrlRegs.GPBDIR.bit.GPIO52=0;
       GpioCtrlRegs.GPBDIR.bit.GPIO40=1;


       AdcaRegs.ADCSOCFRC1.bit.SOC0 = 1;
       while (AdcaRegs.ADCCTL1.bit.ADCBSY == 1) {
       // Wait until channel is available
       }
    value=ADC_readResult(ADCARESULT_BASE,ADC_SOC_NUMBER0);
    //**********************************************
    xt=value*0.216-70;
      }

    //ADC TWO***************************************
    // ready();
      for(long b=0;b<1000;b++){
      EALLOW;

    GpioDataRegs.GPDDAT.bit.GPIO111=0;
     GpioDataRegs.GPBDAT.bit.GPIO52=0;
     GpioDataRegs.GPBDAT.bit.GPIO40=0;
     GpioCtrlRegs.GPDDIR.bit.GPIO111=0;
     GpioCtrlRegs.GPBDIR.bit.GPIO56=1;
     GpioCtrlRegs.GPBDIR.bit.GPIO52=1;
     GpioCtrlRegs.GPBDIR.bit.GPIO40=0;
     GpioDataRegs.GPBDAT.bit.GPIO56=1;

      AdcbRegs.ADCSOCFRC1.bit.SOC1 = 1;
              while (AdcbRegs.ADCCTL1.bit.ADCBSY == 1) {
              // Wait until channel is available
              }
          value2=ADC_readResult(ADCBRESULT_BASE,ADC_SOC_NUMBER1);
          yt=(value2-950)*0.204;
          yt=-(yt-480);
      }
         //*************************************************

      //Ping pong to get rid of outliers
          buffer0[state]=xt;
          buffer1[state]=yt;
          state++;
          if(xt>0){bootuptime++;}
          if(state>=5)
          {state=0;}
          //compute local average
          local1=buffer0[0]+buffer0[1]+buffer0[2]+buffer0[3]+buffer0[4];
          local1=local1/5;

          local2=buffer1[0]+buffer1[1]+buffer1[2]+buffer1[3]+buffer1[4];
          local2=local2/5;


     if(xt>0 & bootuptime>=5 ){
         if(local2<395 & local2>5 &local1<680 & on==1){
             temp2=local1;
             local1=local1/divider;
             local1=local1*divider;
     FillUpBlock(local1,0,25,temp5,0b11111111);
     FillUpBlock(local1,local2,25,400-local2,touchcolor);
     bootuptime=5; //so that the first 5 values (and only the first five values) AFTER the screen is touched are ignored}
     temp5=local2;
     store();
     }}
         if(xt<0){
             bootuptime=0; //reset bootup time if screen is not touched
         }
  }

  void FillUpBlock(Uint16 x, Uint16 y, Uint32 width, Uint32 height,Uint16 c1)
  {
       Uint32 size=width*4*height;

       Uint16 SLBx= (x & 0x00FF);
       Uint16 SHBx= (x & 0xFF00)>>8;
       Uint16 ELBx= ((x+width) & 0x00FF);
       Uint16 EHBx= ((x+width) & 0xFF00)>>8;

       Uint16 SLBy= (y & 0x00FF);
       Uint16 SHBy= (y & 0xFF00)>>8;
       Uint16 ELBy= ((y+height) & 0x00FF);
       Uint16 EHBy= ((y+height) & 0xFF00)>>8;

      //Horizontal block of dimension 20*50
      writeCommand(0x2A); //To set column address
      writeData(SHBx); //SC
      writeData(SLBx); //SC
      writeData(EHBx); //EC
      writeData(ELBx); //EC
      writeCommand(0x2B); //To set page address
      writeData(SHBy); //SC
      writeData(SLBy); //SC
      writeData(EHBy); //EC
      writeData(ELBy); //EC

          //fill up the block
      if(size<1540000){
      writeCommand(0x2C);
      for(Uint32 j=0; j<size;j++){
      writeData(c1);
      }}

  }

  void readx()
  {
      EALLOW;
      GpioCtrlRegs.GPBDIR.bit.GPIO52=0; //setting 52 and 56 to float
      GpioCtrlRegs.GPBDIR.bit.GPIO56=0; //
      GpioCtrlRegs.GPBPUD.bit.GPIO52=1;
      GpioCtrlRegs.GPBPUD.bit.GPIO56=1;

      GpioCtrlRegs.GPBDIR.bit.GPIO40=1; //setting output
      GpioCtrlRegs.GPDDIR.bit.GPIO111=1; //setting output
      GpioDataRegs.GPBDAT.bit.GPIO40=0; //setting 0 on pin 10
      GpioDataRegs.GPDDAT.bit.GPIO111=1; //setting 3.3 in pin 14
  }

  void ready()
  {
      EALLOW;
      GpioCtrlRegs.GPBDIR.bit.GPIO40=0; //
      GpioCtrlRegs.GPDDIR.bit.GPIO111=0; //

      GpioCtrlRegs.GPBGMUX2.bit.GPIO52 = 0;
      GpioCtrlRegs.GPBMUX2.bit.GPIO52 = 0;
      GpioCtrlRegs.GPBGMUX2.bit.GPIO56 = 0;
      GpioCtrlRegs.GPBMUX2.bit.GPIO56 = 0;

      GpioCtrlRegs.GPBDIR.bit.GPIO56=1; //
      GpioCtrlRegs.GPBDIR.bit.GPIO52=1; //
      GpioDataRegs.GPBDAT.bit.GPIO52=0; //setting 0 on pin 52
      GpioDataRegs.GPBDAT.bit.GPIO56=1; //setting 3.3 in pin 56
  }

  void adcInitialize()
  {
      //ADC initialize for touchscreen
      ADC_setPrescaler( ADCA_BASE, ADC_CLK_DIV_1_0);
      ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT,ADC_MODE_SINGLE_ENDED);
      ADC_enableConverter(ADCA_BASE);
      DELAY_US(5E3);  //delay to let the adc boot up
      ADC_setPrescaler( ADCB_BASE, ADC_CLK_DIV_1_0);
          ADC_setMode(ADCB_BASE, ADC_RESOLUTION_12BIT,ADC_MODE_SINGLE_ENDED);
          ADC_enableConverter(ADCB_BASE);
      DELAY_US(5E3);  //delay to let the adc boot up


      ADC_setupSOC(ADCA_BASE,  ADC_SOC_NUMBER0, ADC_TRIGGER_SW_ONLY,ADC_CH_ADCIN0 ,400);
      ADC_setupSOC(ADCB_BASE,  ADC_SOC_NUMBER1, ADC_TRIGGER_SW_ONLY,ADC_CH_ADCIN2,400);

      }

  void writeLetter(Uint16 x, Uint16 y, Uint16 size, Uint16 address, Uint16 color1, Uint16 color2)
  {
      rBlock = readBlock(address);
      int add=0;
      Uint16 multiplier1=size/10;
      Uint16 multiplier2=size/(100);
      Uint16 SLBx= (x & 0x00FF);
      Uint16 SHBx= (x & 0xFF00)>>8;
      Uint16 ELBx= ((x+(size-1)) & 0x00FF);
      Uint16 EHBx= ((x+(size-1)) & 0xFF00)>>8;

      Uint16 SLBy= (y & 0x00FF);
      Uint16 SHBy= (y & 0xFF00)>>8;
      Uint16 ELBy= ((y+(size-1)) & 0x00FF);
      Uint16 EHBy= ((y+(size-1)) & 0xFF00)>>8;

      writeCommand(0x2A); //To set column address
      writeData(SHBx); //SC
      writeData(SLBx); //SC
      writeData(EHBx); //EC
      writeData(ELBx); //EC
      writeCommand(0x2B); //To set page address
      writeData(SHBy); //SC
      writeData(SLBy); //SC
      writeData(EHBy); //EC
      writeData(ELBy); //EC
      //writeCommand(0x2C);
     /* for(int v=0;v<300;v++){
      for(int c=0;c<102;c++)
      {writeData(0x00);}}
  */
      //fill up the block
        writeCommand(0x2C);

      for(int g=0;g<(10);g++){
      for(int k=0;k<(multiplier1);k++)
      {
      for(Uint16 j=0+add; j<30+add;j++){
          if(rBlock[j]==1){
                  for(int c=0;c<multiplier1;c++)
                  {writeData(color1);}
              }
              else if(rBlock[j]==0){
                  for(int c=0;c<multiplier1;c++)
                  {writeData(color2);}}
      }}
   add=add+30;
  }

  }

  void secondary1(){

      transfercalculation(csetup.oct);
      FillUpBlock(700,0,15,100,0);
      FillUpBlock(700,135,15,120,0);
      FillUpBlock(700,12,20,6,0b11111111);
      writeLetter(720,160,20,30+num1,0b0,0b11111111);
      writeLetter(735,160,20,30+num2,0b0,0b11111111);
      writeLetter(750,160,20,30+num3,0b0,0b11111111);
      writeLetter(765,160,20,30+num4,0b0,0b11111111);
      transfercalculation(csetup.freq);
      writeLetter(720,215,20,30+num1,0b0,0b11111111);
      writeLetter(735,215,20,30+num2,0b0,0b11111111);
      writeLetter(750,215,20,30+num3,0b0,0b11111111);
      writeLetter(765,215,20,30+num4,0b0,0b11111111);
      if(minmaxflag==0){rotarystate2=csetup.oct;
      FillUpBlock(700,138,20,6,0b11111111);}
      else if(minmaxflag==-1){rotarystate2=csetup.freq;
      FillUpBlock(700,193,20,6,0b11111111);}

      FillUpBlock(720,135,80,120,0b11111111);

      rotarystate3=csetup.arpspeed;
      //OCT
      writeLetter(720,135,20,15,0b0,0b11111111);
      writeLetter(735,135,20,3,0b0,0b11111111);
      writeLetter(750,135,20,20,0b0,0b11111111);
      transfercalculation(csetup.oct);
      writeLetter(720,160,20,30+num1,0b0,0b11111111);
      writeLetter(735,160,20,30+num2,0b0,0b11111111);
      writeLetter(750,160,20,30+num3,0b0,0b11111111);
      writeLetter(765,160,20,30+num4,0b0,0b11111111);

      //FREQ
      writeLetter(720,190,20,6,0b0,0b11111111);
      writeLetter(735,190,20,18,0b0,0b11111111);
      writeLetter(750,190,20,5,0b0,0b11111111);
      writeLetter(765,190,20,17,0b0,0b11111111);
      transfercalculation(csetup.freq);
      writeLetter(720,215,20,30+num1,0b0,0b11111111);
      writeLetter(735,215,20,30+num2,0b0,0b11111111);
      writeLetter(750,215,20,30+num3,0b0,0b11111111);
      writeLetter(765,215,20,30+num4,0b0,0b11111111);


      for(int g=0; g<23; g++){
          if(GpioDataRegs.GPCDAT.bit.GPIO94==0){break;}
          FillUpBlock(g*30,0,25,400-csetup.arp[g],0b11111111);
          if(GpioDataRegs.GPCDAT.bit.GPIO94==0){break;}
          FillUpBlock(g*30,400-csetup.arp[g],25,csetup.arp[g],touchcolor);
      }

  }

  void secondary2(){
      FillUpBlock(720,135,80,120,0b11111111);

      //SUST
      writeLetter(720,160,20,19,0b0,0b11111111);
      writeLetter(735,160,20,21,0b0,0b11111111);
      writeLetter(750,160,20,19,0b0,0b11111111);
      writeLetter(765,160,20,20,0b0,0b11111111);
      //POS
      writeLetter(720,185,20,16,0b0,0b11111111);
      writeLetter(735,185,20,15,0b0,0b11111111);
      writeLetter(750,185,20,19,0b0,0b11111111);
      rotarystate3=csetup.volspeed;
      rotarystate2=csetup.sustain;
      transfercalculation(csetup.sustain);
      FillUpBlock(0,400,715,15,0);
      FillUpBlock(30*csetup.sustain+12,400,5,15,0b11111111);
      FillUpBlock(700,0,15,100,0);
      FillUpBlock(700,135,15,120,0);
      FillUpBlock(700,37,20,6,0b11111111);
      writeLetter(720,210,20,30+num3,0b0,0b11111111);
      writeLetter(735,210,20,30+num4,0b0,0b11111111);

      for(int g=0; g<23; g++){
          if(GpioDataRegs.GPCDAT.bit.GPIO94==0){break;}
          FillUpBlock(g*30,0,25,400-csetup.vol[g],0b11111111);
          if(GpioDataRegs.GPCDAT.bit.GPIO94==0){break;}
          FillUpBlock(g*30,400-csetup.vol[g],25,csetup.vol[g],touchcolor);
      }
  }

  void secondary3(){
      FillUpBlock(700,135,15,120,0);
      rotarystate2=csetup.unitemp;
      rotarystate3=csetup.unispeed;
      temp4=-1;
      FillUpBlock(700,0,15,100,0);
      FillUpBlock(700,62,20,6,0b11111111);

      FillUpBlock(720,135,80,120,0b11111111);
      //1
      writeLetter(720,135,20,31,0b0,0b11111111);
      //2
      writeLetter(720,160,20,32,0b0,0b11111111);
      //3
      writeLetter(720,185,20,33,0b0,0b11111111);

      //writeLetter(720,210,20,37,0b0,0b11111111); //Coming out soon
      for(int g=0; g<23; g++){
          if(GpioDataRegs.GPCDAT.bit.GPIO94==0){break;}
          FillUpBlock(g*30,0,25,400-csetup.uni[g],0b11111111);
          if(GpioDataRegs.GPCDAT.bit.GPIO94==0){break;}
          FillUpBlock(g*30,400-csetup.uni[g],25,csetup.uni[g],touchcolor);
      }
  }

  void secondary4(){

      transfercalculation(csetup.min);
      FillUpBlock(700,0,15,100,0);
      FillUpBlock(700,135,15,120,0);
      FillUpBlock(700,87,20,6,0b11111111);
      writeLetter(720,160,20,30+num1,0b0,0b11111111);
      writeLetter(735,160,20,30+num2,0b0,0b11111111);
      writeLetter(750,160,20,30+num3,0b0,0b11111111);
      writeLetter(765,160,20,30+num4,0b0,0b11111111);
      transfercalculation(csetup.max);
      writeLetter(720,215,20,30+num1,0b0,0b11111111);
      writeLetter(735,215,20,30+num2,0b0,0b11111111);
      writeLetter(750,215,20,30+num3,0b0,0b11111111);
      writeLetter(765,215,20,30+num4,0b0,0b11111111);
      if(minmaxflag==0){rotarystate2=csetup.min;
      FillUpBlock(700,138,20,6,0b11111111);}
      else if(minmaxflag==-1){rotarystate2=csetup.max;
      FillUpBlock(700,193,20,6,0b11111111);}

      FillUpBlock(720,135,80,120,0b11111111);

      rotarystate3=csetup.filspeed;
      //Min frequency
      writeLetter(720,135,20,13,0b0,0b11111111);
      writeLetter(735,135,20,9,0b0,0b11111111);
      writeLetter(750,135,20,14,0b0,0b11111111);
      transfercalculation(csetup.min);
      writeLetter(720,160,20,30+num1,0b0,0b11111111);
      writeLetter(735,160,20,30+num2,0b0,0b11111111);
      writeLetter(750,160,20,30+num3,0b0,0b11111111);
      writeLetter(765,160,20,30+num4,0b0,0b11111111);

      //Max frequency
      writeLetter(720,190,20,13,0b0,0b11111111);
      writeLetter(735,190,20,1,0b0,0b11111111);
      writeLetter(750,190,20,24,0b0,0b11111111);
      transfercalculation(csetup.max);
      writeLetter(720,215,20,30+num1,0b0,0b11111111);
      writeLetter(735,215,20,30+num2,0b0,0b11111111);
      writeLetter(750,215,20,30+num3,0b0,0b11111111);
      writeLetter(765,215,20,30+num4,0b0,0b11111111);


      for(int g=0; g<23; g++){
          if(GpioDataRegs.GPCDAT.bit.GPIO94==0){break;}
          FillUpBlock(g*30,0,25,400-csetup.fil[g],0b11111111);
          if(GpioDataRegs.GPCDAT.bit.GPIO94==0){break;}
          FillUpBlock(g*30,400-csetup.fil[g],25,csetup.fil[g],touchcolor);
      }
  }

  void crossoutcheck()
  {
      if(csetup.filTrue==true){
          //Fil
          FillUpBlock(730,85,60,20,0b11111111);
          writeLetter(730,85,20,6,0b0,0b11111111);
          writeLetter(750,85,20,9,0b0,0b11111111);
          writeLetter(770,85,20,12,0b0,0b11111111);
      }
      else {
          writeLetter(730,85,20,6,0b0,0b11111111);
          writeLetter(750,85,20,9,0b0,0b11111111);
          writeLetter(770,85,20,12,0b0,0b11111111);
          FillUpBlock(730,93,60,4,0);  //Crossed out
      }

      if(csetup.arpTrue==true){
          //Fil
          FillUpBlock(730,10,60,20,0b11111111);
          writeLetter(730,10,20,1,0b0,0b11111111);
          writeLetter(750,10,20,18,0b0,0b11111111);
          writeLetter(770,10,20,16,0b0,0b11111111);
      }
      else {
          writeLetter(730,10,20,1,0b0,0b11111111);
          writeLetter(750,10,20,18,0b0,0b11111111);
          writeLetter(770,10,20,16,0b0,0b11111111);
          FillUpBlock(730,18,60,4,0);  //Crossed out
      }
  }

  void startmenu()
  {
          //Synthe shaper (hype)
                 writeLetter(100,75,50,19,0b0,0b11111111);
                 writeLetter(150,75,50,25,0b0,0b11111111);
                 writeLetter(200,75,50,20,0b0,0b11111111);
                 writeLetter(250,75,50,8,0b0,0b11111111);
                 writeLetter(300,75,50,5,0b0,0b11111111);

                 FillUpBlock(352,95,40,5,0); //dash

                 writeLetter(400,75,50,19,0b0,0b11111111);
                 writeLetter(450,75,50,8,0b0,0b11111111);
                 writeLetter(500,75,50,1,0b0,0b11111111);
                 writeLetter(550,75,50,16,0b0,0b11111111);
                 writeLetter(600,75,50,5,0b0,0b11111111);
                 writeLetter(650,75,50,18,0b0,0b11111111);

                 //START
             writeLetter(250,200,50,19,0b0,0b11111111);
             writeLetter(300,200,50,20,0b0,0b11111111);
             writeLetter(350,200,50,1,0b0,0b11111111);
             writeLetter(400,200,50,18,0b0,0b11111111);
             writeLetter(450,200,50,20,0b0,0b11111111);

             draw(0);
             while(local1<200 || local1>320 || local2<130 || local2>370){ //start position
             draw(0);
             animation++;
             if(animation>150){
                 animation=0;
                 if(animationtemp==0){
                 writeCommand(0x21);
                 animationtemp=1;}
                 else if(animationtemp==1){
                     writeCommand(0x20);
                     animationtemp=0;}
             }
             }

             while(local1>0){
                 draw(0);
             } // wait til release
             draw(0);
             menuInit();
  }

  void menuInit(){
      FillUpBlock(0,75,750,500,0b11111111);
      FillUpBlock(0,400,715,15,0);

      FillUpBlock(700,0,15,480,0);
      FillUpBlock(715,120,80,10,0);
      FillUpBlock(715,260,80,10,0);
      FillUpBlock(715,390,80,10,0);

      //arp
      writeLetter(730,10,20,1,0b0,0b11111111);
      writeLetter(750,10,20,18,0b0,0b11111111);
      writeLetter(770,10,20,16,0b0,0b11111111);
      FillUpBlock(730,18,60,4,0);  //Crossed out by default
      //Vol
      writeLetter(730,35,20,22,0b0,0b11111111);
      writeLetter(750,35,20,15,0b0,0b11111111);
      writeLetter(770,35,20,12,0b0,0b11111111);
      //Uni
      writeLetter(730,60,20,21,0b0,0b11111111);
      writeLetter(750,60,20,14,0b0,0b11111111);
      writeLetter(770,60,20,9,0b0,0b11111111);
      //Fil default is crossed out
      writeLetter(730,85,20,6,0b0,0b11111111);
      writeLetter(750,85,20,9,0b0,0b11111111);
      writeLetter(770,85,20,12,0b0,0b11111111);
      FillUpBlock(730,93,60,4,0);  //Crossed out

      secondary1();

      //Speed
      writeLetter(720,290,20,19,0b0,0b11111111);
      writeLetter(735,290,20,16,0b0,0b11111111);
      writeLetter(750,290,20,5,0b0,0b11111111);
      writeLetter(765,290,20,5,0b0,0b11111111);
      writeLetter(780,290,20,4,0b0,0b11111111);

      //speed bar
      FillUpBlock(760+3*rotarystate3,320,2,40,0);
      menu=0;

      //Save
      writeLetter(720,405,20,19,0b0,0b11111111);
      writeLetter(735,405,20,1,0b0,0b11111111);
      writeLetter(750,405,20,22,0b0,0b11111111);
      writeLetter(765,405,20,5,0b0,0b11111111);

      //Exit
      writeLetter(720,455,20,5,0b0,0b11111111);
      writeLetter(735,455,20,24,0b0,0b11111111);
      writeLetter(750,455,20,9,0b0,0b11111111);
      writeLetter(765,455,20,20,0b0,0b11111111);

      //Loop
     /* writeLetter(10,430,20,12,0b0,0b11111111);
      writeLetter(25,430,20,15,0b0,0b11111111);
      writeLetter(40,430,20,15,0b0,0b11111111);
      writeLetter(55,430,20,16,0b0,0b11111111);*/

      FillUpBlock(100,400,5,80,0); //loop bar
      temp=-1;
      temp4=-1;
      //default Osc selection
      //FillUpBlock(700,12,20,6,0b11111111);
      //default square wave selection
      //FillUpBlock(700,135,20,6,0b11111111);
      if(csetup.filTrue==true){
                       //Fil
                       FillUpBlock(730,85,60,20,0b11111111);
                       writeLetter(730,85,20,6,0b0,0b11111111);
                       writeLetter(750,85,20,9,0b0,0b11111111);
                       writeLetter(770,85,20,12,0b0,0b11111111);
                   }
                   else {
                       writeLetter(730,85,20,6,0b0,0b11111111);
                       writeLetter(750,85,20,9,0b0,0b11111111);
                       writeLetter(770,85,20,12,0b0,0b11111111);
                       FillUpBlock(730,93,60,4,0);  //Crossed out
                   }

                   if(csetup.arpTrue==true){
                       //Fil
                       FillUpBlock(730,10,60,20,0b11111111);
                       writeLetter(730,10,20,1,0b0,0b11111111);
                       writeLetter(750,10,20,18,0b0,0b11111111);
                       writeLetter(770,10,20,16,0b0,0b11111111);
                   }
                   else {
                       writeLetter(730,10,20,1,0b0,0b11111111);
                       writeLetter(750,10,20,18,0b0,0b11111111);
                       writeLetter(770,10,20,16,0b0,0b11111111);
                       FillUpBlock(730,18,60,4,0);  //Crossed out
                   }

  }

  void save(){

      //SAVE?
      writeLetter(100,75,50,19,0b0,0b11111111);
      writeLetter(150,75,50,1,0b0,0b11111111);
      writeLetter(200,75,50,22,0b0,0b11111111);
      writeLetter(250,75,50,5,0b0,0b11111111);
      //writeLetter(300,75,50,5,0b0,0b11111111);

      //Folder
      writeLetter(100,120,50,6,0b0,0b11111111);
      writeLetter(150,120,50,15,0b0,0b11111111);
      writeLetter(200,120,50,12,0b0,0b11111111);
      writeLetter(250,120,50,4,0b0,0b11111111);
      writeLetter(300,120,50,5,0b0,0b11111111);
      writeLetter(350,120,50,18,0b0,0b11111111);

      //Folders 1 2 3 4
      writeLetter(470,120,50,31,0b0,0b11111111);
      writeLetter(540,120,50,32,0b0,0b11111111);
      writeLetter(610,120,50,33,0b0,0b11111111);
      writeLetter(680,120,50,34,0b0,0b11111111);

      while(local1<450 || local1>700 || local2<100 || local2>200){ //1 2 3 4 position
      draw(0);
      animation++;
      if(animation>150){
          animation=0;
          if(animationtemp==0){
          writeCommand(0x21);
          animationtemp=1;}
          else if(animationtemp==1){
              writeCommand(0x20);
              animationtemp=0;}
      }}
      draw(0);
      if(local1>460 & local1<510){
          folder=0;
      }
      else if(local1>530 & local1<580){
          folder=1;
      }
      else if(local1>590 & local1<650){
          folder=2;
      }
      else if(local1>670 & local1<730){
          folder=3;
      }

      while(local1>450 & local1<510 & local2>100 & local2<200){
          draw(0);
      } // wait til release


      //Confirm ?
      writeLetter(50,200,50,3,0b0,0b11111111);
      writeLetter(100,200,50,15,0b0,0b11111111);
      writeLetter(150,200,50,14,0b0,0b11111111);
      writeLetter(200,200,50,6,0b0,0b11111111);
      writeLetter(250,200,50,9,0b0,0b11111111);
      writeLetter(300,200,50,18,0b0,0b11111111);
      writeLetter(350,200,50,13,0b0,0b11111111);
      writeLetter(200,280,50,30+folder+1,0b0,0b11111111);

      FillUpBlock(410,200,10,50,0);

      //Cancel ?
      writeLetter(450,200,50,3,0b0,0b11111111);
      writeLetter(500,200,50,1,0b0,0b11111111);
      writeLetter(550,200,50,14,0b0,0b11111111);
      writeLetter(600,200,50,3,0b0,0b11111111);
      writeLetter(650,200,50,5,0b0,0b11111111);
      writeLetter(700,200,50,12,0b0,0b11111111);

  //debouncing the touchscreen
          while(local1>0){
              draw(0);
          }
       draw(0);

       while(local1<50 || local1>700 || local2<200 || local2>300){ //1 2 3 4 position
       draw(0);
       animation++;
       if(animation>300){
           animation=0;
           if(animationtemp==0){
           writeCommand(0x21);
           animationtemp=1;}
           else if(animationtemp==1){
               writeCommand(0x20);
               animationtemp=0;}
       }}
       draw(0);

      if(local1>330) {cancel=1;}
      else {cancel=0;}

      //store stuff if cancel is 0
      if(cancel==0){
          //add stuff to make it easier for wesley
          fol[folder]=csetup; //store current setup to folder selected
      }
      //else just ignore

      menuInit();

      while(local1>0){
          draw(0);
      } // wait til release
      draw(0);
  }

  void transfercalculation(Uint16 number)
  {
      //example... number = 1536
      float transfer=number; //transfer=1.536

      if(rotarystate!=1){
      transfer=transfer/1000+0.0001; //transfer=1.53599999

      num1=transfer;     //min1=1
      transfer=transfer-num1; //transfer=1.536-1=0.536;
      transfer=transfer*10; //transfer=5.36
      num2=transfer; //min2=5
      transfer=transfer-num2; //transfer=5.36-5=0.36
      transfer=transfer*10; //transfer=3.6
      num3=transfer; //min3=3
      transfer=transfer-num3; //transfer=3.6-3=0.6
      transfer=transfer*10; //transfer=6
      num4=transfer; }//min4=6
      else {//example transfer=15
          transfer=transfer/10+0.01; //transfer=1.49999 +.01= 1.5
          num3=transfer; //min3=1
          transfer=transfer-num3; //transfer=0.5
          transfer=transfer*10; //transfer=5
          num4=transfer; //5
          }

  }

  void store() {
      Uint16 pos;
      pos=local1/divider; //where to write to the array (0 to 22) normally
      if(rotarystate==0){csetup.arp[pos]=400-local2;}

      else if(rotarystate==1){csetup.vol[pos]=400-local2;}

      else if(rotarystate==2){csetup.uni[pos]=400-local2;}

      else if(rotarystate==3){csetup.fil[pos]=400-local2;}
  }


  void writeCommand(Uint16 command) //For commands, it is only 8 bit (D7-D0 -->lowest 8 bits of portA)
  {
      //change and filter data to have it ready in the middle
      EALLOW;
      GpioDataRegs.GPADAT.all=command;
      //sdelay();
      GpioDataRegs.GPDDAT.bit.GPIO105 = 0; //CS LOW
      //for (int16 i = 0; i < 1; i++) {}
      GpioDataRegs.GPADAT.bit.GPIO22 = 0; //D/C low
      //for (int16 i = 0; i < 1; i++) {}
      GpioDataRegs.GPCDAT.bit.GPIO67 = 0; //WR low
      //for (int16 i = 0; i < 1; i++) {}
      //have data ready here (valid data)
      GpioDataRegs.GPCDAT.bit.GPIO67 = 1; //WR high
      //sdelay();
      GpioDataRegs.GPADAT.bit.GPIO22 = 1; //D/C high
      GpioDataRegs.GPDDAT.bit.GPIO105 = 1; //CS high
      //sdelay();

   }

  void writeData(Uint16 data) // Command (8 bit parameters), data(16-bit interface)
  {
      //change and filter data to have it ready in the middle
      EALLOW;
      GpioDataRegs.GPADAT.all=data;
      //for (int16 i = 0; i < 1; i++) {}
      GpioDataRegs.GPDDAT.bit.GPIO105 = 0; //CS LOW
      //for (int16 i = 0; i < 1; i++) {}
      GpioDataRegs.GPADAT.bit.GPIO22 = 1; //D/C High since we are writing data
      //for (int16 i = 0; i < 1; i++) {}
      GpioDataRegs.GPCDAT.bit.GPIO67 = 0; //WR low
      //for (int16 i = 0; i < 1; i++) {}
      //have data ready here (valid data)
      GpioDataRegs.GPCDAT.bit.GPIO67 = 1; //WR high
      //for (int16 i = 0; i < 1; i++) {}
      GpioDataRegs.GPADAT.bit.GPIO22 = 1; //D/C high
      //for (int16 i = 0; i < 1; i++) {}
      GpioDataRegs.GPDDAT.bit.GPIO105 = 1; //CS high
      //sdelay();

   }

  //Software sdelay for the lcd, the largest time is 13*2 ns for read
  void sdelay()
  {
      for (int16 i = 0; i < 1; i++) {}
  }

  void ldelay()
  {
      long i;
          for (i = 0; i < 1500; i++) {}
      }


  void GPIO()
  {
      InitSysCtrl();
          EALLOW;

          // Chip select, Data/Command, and Write
      GpioCtrlRegs.GPDGMUX1.bit.GPIO105 = 0x0; //CS
      GpioCtrlRegs.GPDMUX1.bit.GPIO105 = 0x0;
      GpioCtrlRegs.GPDDIR.bit.GPIO105 = 1; //output
      GpioDataRegs.GPDDAT.bit.GPIO105 = 1;


      GpioCtrlRegs.GPCGMUX1.bit.GPIO67 = 0x0; // /WR
      GpioCtrlRegs.GPCMUX1.bit.GPIO67 = 0x0;
      GpioCtrlRegs.GPCDIR.bit.GPIO67 = 1; //output
      GpioDataRegs.GPCDAT.bit.GPIO67 = 1;

      //PIN CHANGE
      GpioCtrlRegs.GPBGMUX1.bit.GPIO32 = 0x0; //Reset
      GpioCtrlRegs.GPBMUX1.bit.GPIO32 = 0x0;
      GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1; //output
      GpioDataRegs.GPBDAT.bit.GPIO32 = 1;

      //DATA BUS 16 bit parallel on port A
      GpioCtrlRegs.GPAGMUX1.all = 0;
      GpioCtrlRegs.GPAMUX1.all = 0;
      GpioCtrlRegs.GPAGMUX2.all = 0;
      GpioCtrlRegs.GPAMUX2.all = 0;
      GpioCtrlRegs.GPADIR.all=0xFFFFFFFF;
      GpioCtrlRegs.GPAPUD.all=0xFFFFFFFF;


      GpioCtrlRegs.GPAGMUX2.bit.GPIO22 = 0x0; //D/C
      GpioCtrlRegs.GPAMUX2.bit.GPIO22 = 0x0;
      GpioCtrlRegs.GPADIR.bit.GPIO22 = 1; //output
      GpioDataRegs.GPADAT.bit.GPIO22 = 1;

      EALLOW;

      GpioCtrlRegs.GPCDIR.bit.GPIO94 = 0;//rotary input clk
      GpioCtrlRegs.GPCDIR.bit.GPIO95 = 0;//rotary input dat

      GpioCtrlRegs.GPDDIR.bit.GPIO97 = 0;//rotary input clk
      GpioCtrlRegs.GPADIR.bit.GPIO9 = 0;//rotary input dat

      GpioCtrlRegs.GPADIR.bit.GPIO16 = 0;//rotary input clk
      GpioCtrlRegs.GPADIR.bit.GPIO8 = 0;//rotary input dat

      GpioCtrlRegs.GPBDIR.bit.GPIO41 = 0; //input clear button
      GpioCtrlRegs.GPADIR.bit.GPIO29 = 0; //input min/max switching button


      GpioCtrlRegs.GPADIR.bit.GPIO10 = 0; //Folder 1
      GpioCtrlRegs.GPADIR.bit.GPIO11 = 0; //folder 2
      GpioCtrlRegs.GPADIR.bit.GPIO14 = 0; //folder 3
      GpioCtrlRegs.GPADIR.bit.GPIO15 = 0; //folder 4

      GpioCtrlRegs.GPEDIR.bit.GPIO130 = 0;//filter bool
  }
