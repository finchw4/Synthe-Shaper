// standard includes
#include <F28x_Project.h>
#include <stdlib.h>
#include <math.h>
// MCBSPB include
#include <AIC23.c>
#include <AIC23.h>
#include <initAIC23.c>
#include <interrupt.h>
#include <interrupt.c>
// MIDI includes
#include "MIDI.h"

/******************************************************************/
                   /*VARIABLES AND PROTOTYPES*/
/******************************************************************/

// MCBSPB
interrupt void Mcbsp_RxINTB_ISR(void);
volatile int ch_sel = 1;

const long volume = 50000000;
const long fsamp = 24000;
const float noteMul = 0.0594;
int squareCutoff[4];
int unisonCutoff[4][2];

volatile uint16_t numNotes = 0;
volatile float arpIdx = 0;
volatile float gatePercent = 0;
volatile int gateIdx = 0;
volatile int octIdx = 0;

volatile uint32_t SoundData;

volatile bool go = false;

// MIDI
void betterCheckMIDI(void);

volatile struct Note note_arr[4];
volatile struct Note released_arr[4];
volatile struct Note blankNote;
volatile int MIDIcount = 0;
volatile int status;
volatile int note;
volatile int velocity;
volatile int msg;
volatile bool adding = false;
volatile bool removing = false;
volatile bool isStatus = false;

// FILTER
void initFilter(void);
volatile int16 DataInLeft;
volatile int16 DataInRight;
volatile int16 samplein;
Uint16 Fw;
volatile Uint16 filtNum = 0;
volatile bool filtFlag = false;
volatile float yh1=0;
volatile float yh2=0;
volatile float yb1=0;
volatile float yb2=0;
volatile float yl1=0;
volatile float yl2=0;
volatile float F1=0;
volatile float Q1=0;
volatile float Fc[256];
volatile Uint16 upndown;
volatile float delta;
Uint16  y;
Uint16 minf;
Uint16 maxf;
Uint16 centerFreq = 100;
volatile int filIdx = 0;

// INSTRUMENTS

typedef struct SETUP
{
   //arpeggio
   bool arpTrue; //if true then have arp
   int16 oct;
   Uint32 freq;
   float arp[23];
   Uint32 arpspeed;

   //Volume
   int16 sustain; //sustain position
   uint32_t vol[23];
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
}csetup2;  //current setup and folders

volatile csetup2 csetup;
volatile uint32_t oscCount = 0;
#pragma DATA_SECTION(csetupCPU1, "SHARERAMGS0")
volatile csetup2 csetupCPU1[0];

/******************************************************************/
                            /*MAIN*/
/******************************************************************/

int main(void){

    // ENVELOPE
    csetup.volspeed = fsamp >> 5;
    csetup.sustain = 1;
    for(int i = 0; i < 23; i++){
        csetup.vol[i] = volume - (2100000*i);
    }

    // UNISON
    csetup.unitemp = 3;
    for(int i = 0; i < 23; i++){
        csetup.uni[i] = 0.1;
    }
    csetup.unispeed = fsamp >> 2;
    /*for(float i = 0; i < 12; i++){
        csetup.uni[(int)i] = i/25;
    }
    for(float i = 12; i < 23; i++){
        csetup.uni[(int)i] = (22.0 - i)/25;
    }*/

    // FILTER
    csetup.filTrue = false;
    for(int i = 0; i < 12; i++){
        csetup.fil[i] = 200*(i+1); // 2400
    }
    for(int i = 12; i < 23; i++){
        csetup.fil[i] = 200*(23-i); // 2200
    }
    csetup.filspeed = fsamp >> 4;

    // ARPEGGIO
    csetup.arpTrue = true;
    csetup.freq = fsamp >> 2;
    csetup.oct = 3;
    csetup.arpspeed = fsamp >> 2;
    for(int i = 0; i < 23; i++){
        /*if(i < 12){
            csetup.arp[i] = i*0.091;
        }
        else{
            csetup.arp[i] = 1 - ((i-12)*0.091);
        }*/
        csetup.arp[i] = 0.8;
    }

    // MCBSPB setup
    EALLOW;
    InitSysCtrl();
    InitMcBSPb();
    Interrupt_initModule();
    Interrupt_initVectorTable();
    Interrupt_register(INT_MCBSPB_RX, &Mcbsp_RxINTB_ISR);
    Interrupt_enable(INT_MCBSPB_RX);
    EINT;

    InitSPIA();
    InitBigBangedCodecSPI();
    InitAIC23();

    // MIDI setup
    initUART();


    // filter setup
    initFilter();

    while(1){
        DINT;
        betterCheckMIDI();
        EINT;
    }
}

/******************************************************************/
                      /*SUBROUTINES*/
/******************************************************************/

void initFilter(){

    // initial values
    F1 = 2*sinf((M_PI*centerFreq)/fsamp);
    Q1 = 0.5;

}

void betterCheckMIDI(){
    if(ScicRegs.SCIRXST.bit.RXRDY == 1){
        msg = ScicRegs.SCIRXBUF.all;

        if(msg  == 0x90){ //a note is being pressed
            adding = true;
            removing = false;
            isStatus = true;
            MIDIcount = 0;
        }
        else if(msg == 0x80){ //a note is being released
            removing = true;
            adding = false;
            isStatus = true;
            MIDIcount = 0;
        }
        else{
            isStatus = false;
        }

        if(!isStatus){
            if(MIDIcount == 0){ // receiving note
                note = msg;
                MIDIcount++;
            }
            else if(MIDIcount == 1){ //  receiving velocity
                velocity = msg;
                MIDIcount = 0;

                if(adding){
                    // create new Note for the key that was just pressed
                    struct Note newNote = {true,note,velocity};
                    newNote.freq = noteToFreq(note);
                    newNote.squareCutoff = 2*(fsamp/(int)newNote.freq);
                    newNote.isSus = false;

                    for(int i = 0; i < 23; i++){
                        //high detune
                        newNote.uniCutoff[0][i] = 2*(fsamp/(int)(newNote.freq*(1.0+(noteMul*csetup.uni[i]))));
                        newNote.arpUniCutoff[0][i] = newNote.uniCutoff[0][i];
                        // low detune
                        newNote.uniCutoff[1][i] = 2*(fsamp/(int)(newNote.freq*(1.0-(noteMul*csetup.uni[i]))));
                        newNote.arpUniCutoff[1][i] = newNote.uniCutoff[1][i];
                        // high middle detune
                        newNote.uniCutoff[2][i] = 2*(fsamp/(int)(newNote.freq*(1.0+(noteMul*csetup.uni[i]/2))));
                        newNote.arpUniCutoff[2][i] = newNote.uniCutoff[2][i];
                        // low middle detune
                        newNote.uniCutoff[3][i] = 2*(fsamp/(int)(newNote.freq*(1.0-(noteMul*csetup.uni[i]/2))));
                        newNote.arpUniCutoff[3][i] = newNote.uniCutoff[3][i];
                    }
                    newNote.arpCutoff = newNote.squareCutoff;

                    // if possible, find a free spot in note_arr
                    bool found = false;
                    for(int i = 0; i < 4; i++){
                        if(note_arr[i].valid == false && found == false){
                            numNotes++;
                            note_arr[i] = newNote;
                            found = true;
                        }
                    }
                 }
                else if (removing){
                    // create a new Note with the parameters of the key that was released
                    volatile struct Note releasedNote = {false,note,velocity};

                    // search and delete the Note
                    for(int i = 0; i < 4; i++){
                        if(note_arr[i].note_num == releasedNote.note_num){
                            numNotes--; //decrement the number of notes
                            note_arr[i].isSus = false;
                            released_arr[i] = note_arr[i];
                            note_arr[i] = blankNote;
                            // move all notes up to avoid gaps in arpeggio
                            for(int j = i; j < 3; j++){
                                note_arr[j] = note_arr[j+1];
                                note_arr[j+1] = blankNote;
                            }
                        }
                    }
                }
            }
        }
    }

    // clear errors when they occur or the SCI will freeze
    if(ScicRegs.SCIRXST.bit.RXERROR == 1){
        if(ScicRegs.SCIRXST.bit.OE){
            note_arr[0] = blankNote;
            note_arr[1] = blankNote;
            note_arr[2] = blankNote;
            note_arr[3] = blankNote;
            adding = false;
            removing = false;
            MIDIcount = 0;
            numNotes = 0;
            //initUART();
        }
        ScicRegs.SCICTL1.bit.SWRESET = 0;
        ScicRegs.SCICTL1.bit.SWRESET = 1;
    }
}

/************************************************************************************************************************/
                                                  /*INTERRUPTS*/
/************************************************************************************************************************/

interrupt void Mcbsp_RxINTB_ISR(void)
{
    // need to keep this part or the interrupt will stop triggering
    McbspbRegs.DRR2.all;
    McbspbRegs.DRR1.all;

    SoundData = 0;

    if(go){

        // calculate data
        for(int i = 0; i < 4; i++){

            //calculate parameter indices
            note_arr[i].envIdx = note_arr[i].envCount/csetup.volspeed;
            released_arr[i].envIdx = released_arr[i].envCount/csetup.volspeed;
            note_arr[i].uniIdx = (note_arr[i].uniCount/csetup.unispeed) % 23;
            released_arr[i].uniIdx = (released_arr[i].uniCount/csetup.unispeed) % 23;
            //note_arr[i].filtIdx = note_arr[i].filtCount/csetup.filspeed % 23;
            //released_arr[i].filtIdx = released_arr[i].filtCount/csetup.filspeed % 23;


            // increment instrument counts if a note is being played
            if(note_arr[i].valid){
                if(!note_arr[i].isSus){
                    note_arr[i].envCount++;
                }
                note_arr[i].uniCount++;
                note_arr[i].filtCount++;
            }
            else{
                note_arr[i].envCount = 0;
                note_arr[i].uniCount = 0;
                note_arr[i].filtCount = 0;
            }
            // repeat for released notes
            if(released_arr[i].valid){
                if(released_arr[i].envIdx < 23){
                    released_arr[i].envCount++;
                    released_arr[i].uniCount++;
                    released_arr[i].filtCount++;
                }
                else{
                    released_arr[i] = blankNote;
                }
            }

            //adjust isSus
            if(note_arr[i].envIdx == csetup.sustain){
                note_arr[i].isSus = true;
            }

            //******************************* ARPEGGIO CALCULATIONS ********************************//

            if(csetup.arpTrue){

                arpIdx = (float)oscCount/csetup.freq;
                gatePercent = arpIdx - ((int)arpIdx);
                octIdx = ((int)arpIdx % (numNotes*csetup.oct)) / numNotes;
                gateIdx = oscCount/csetup.arpspeed % 23;

                if(i == (int)arpIdx % numNotes && gatePercent < csetup.arp[gateIdx]){

                    note_arr[i].arpCutoff = note_arr[i].squareCutoff >> octIdx;

                    for(int j = 0; j < 4; j++){
                        for(int k = 0; k < 23; k++){
                            note_arr[i].arpUniCutoff[j][k] = note_arr[i].uniCutoff[j][k] >> octIdx;
                        }
                    }

                    //******************************* SOUND DATA CALCULATIONS ********************************//

                    // currently pressed square wave calculations
                    if(oscCount % note_arr[i].arpCutoff  < note_arr[i].arpCutoff/2){
                        // attack/decay
                        if(note_arr[i].envIdx < csetup.sustain){
                            SoundData += csetup.vol[note_arr[i].envIdx];
                        }
                        // sustain
                        else if(note_arr[i].isSus){
                            SoundData += csetup.vol[csetup.sustain];
                        }
                    }
                    // unison
                    for(int j = 0; j < csetup.unitemp-1; j++){
                        if(oscCount % note_arr[i].arpUniCutoff[j][note_arr[i].uniIdx]  < note_arr[i].arpUniCutoff[j][note_arr[i].uniIdx]/2){
                            if(note_arr[i].envIdx < csetup.sustain){
                                SoundData += csetup.vol[note_arr[i].envIdx];
                            }
                            // sustain
                            else if(note_arr[i].isSus){
                                SoundData += csetup.vol[csetup.sustain];
                            }
                        }
                    }

                    //******************************* RELEASED DATA CALCULATIONS ********************************//

                    // released key square wave calculations
                    if(oscCount % released_arr[i].arpCutoff  < released_arr[i].arpCutoff/2 && released_arr[i].envIdx < 23){
                        SoundData += csetup.vol[released_arr[i].envIdx];
                    }
                    // unison
                    if(released_arr[i].envIdx < 23 && released_arr[i].valid){
                        for(int j = 0; j < csetup.unitemp-1; j++){
                            if(oscCount % released_arr[i].arpUniCutoff[j][released_arr[i].uniIdx]  < released_arr[i].arpUniCutoff[j][released_arr[i].uniIdx]/2){
                                SoundData += csetup.vol[released_arr[i].envIdx];
                            }
                        }
                    }
                }
            }
            //******************************* STANDARD CALCULATIONS ********************************//

            else{
                //******************************* SOUND DATA CALCULATIONS ********************************//

                // currently pressed square wave calculations
                if(oscCount % note_arr[i].squareCutoff  < note_arr[i].squareCutoff/2){
                    // attack/decay
                    if(note_arr[i].envIdx < csetup.sustain){
                        SoundData += csetup.vol[note_arr[i].envIdx];
                    }
                    // sustain
                    else if(note_arr[i].isSus){
                        SoundData += csetup.vol[csetup.sustain];
                    }
                }
                // unison
                for(int j = 0; j < csetup.unitemp-1; j++){
                    if(oscCount % note_arr[i].uniCutoff[j][note_arr[i].uniIdx]  < note_arr[i].uniCutoff[j][note_arr[i].uniIdx]/2){
                        if(note_arr[i].envIdx < csetup.sustain){
                            SoundData += csetup.vol[note_arr[i].envIdx];
                        }
                        // sustain
                        else if(note_arr[i].isSus){
                            SoundData += csetup.vol[csetup.sustain];
                        }
                    }
                }

                //******************************* RELEASED DATA CALCULATIONS ********************************//

                // released key square wave calculations
                if(oscCount % released_arr[i].squareCutoff  < released_arr[i].squareCutoff/2 && released_arr[i].envIdx < 23){
                    SoundData += csetup.vol[released_arr[i].envIdx];
                }
                // unison
                if(released_arr[i].envIdx < 23 && released_arr[i].valid){
                    for(int j = 0; j < csetup.unitemp-1; j++){
                        if(oscCount % released_arr[i].uniCutoff[j][released_arr[i].uniIdx]  < released_arr[i].uniCutoff[j][released_arr[i].uniIdx]/2){
                            SoundData += csetup.vol[released_arr[i].envIdx];
                        }
                    }
                }
            }
        }


        //******************************* FILTER ********************************//

        if(csetup.filTrue){
            if(filtNum == 0){
                yh2 = SoundData - yl1 - Q1 * yb1;
                yb2 = F1 * yh2 + yb1;
                yl2 = F1 * yb2 + yl1;


                McbspbRegs.DXR2.all = ((int32) yb2) >> 14;
                McbspbRegs.DXR1.all = 0;
                filtNum = 1;
                }
            else if(filtNum == 1){
                yh1 = SoundData - yl2 - Q1 * yb2;
                yb1 = F1 * yh1 + yb2;
                yl1 = F1 * yb1 + yl2;

                McbspbRegs.DXR2.all = ((int32)yb1) >> 14;
                McbspbRegs.DXR1.all = 0;
                filtNum = 0;
            }

            filIdx = (oscCount/csetup.filspeed) % 23;
            F1 = 2*sinf((M_PI*csetup.fil[filIdx])/fsamp);

            /*
            if(note_arr[0].valid){
                F1 = 2*sinf((M_PI*csetup.fil[note_arr[0].filtIdx])/fsamp);
            }
            else{
                F1 = F1;
            }*/
        }
        else{
            McbspbRegs.DXR2.all = SoundData >> 16;
            McbspbRegs.DXR1.all = SoundData;
        }

        // send out data
        //McbspbRegs.DXR2.all = SoundData >> 16;
        //McbspbRegs.DXR1.all = SoundData;

        // increment count
        oscCount++;
        go = false;
    }
    else{
        go = true;
    }


    // acknowledge interrupt
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP6;
}
