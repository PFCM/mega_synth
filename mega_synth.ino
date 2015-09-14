#include <MIDI.h>

#define MOD_NONE 0
#define MOD_PITCH 0x01
#define MOD_AMP 0x10
#define MOD_XOR 0x100

/** envelope states */
#define OFF 0
#define ATTACK 1
#define DECAY 2
#define SUSTAIN 3
#define RELEASE 4

/** envelope variables */
unsigned int attack_time=200;
unsigned int decay_time=2000;
unsigned char sustain_level=127;
unsigned int release_time=2000;

unsigned char osc_states[6] = {
  0,0,0,0,0,0};
unsigned int osc_gains[6] = {
  0,0,0,0,0,0};
unsigned int osc_gainsteps[6] = {
  0,0,0,0,0,0};
unsigned int osc_counters[6] = {
  0,0,0,0,0,0};

char sinewave[256];
char triwave[256];
char squarewave[256];
char sawwave[256];
char pulse[256];
char noise[256];

unsigned char notes[] = {36,38,40,41,43,45,46,48};

extern int midi_lookup[127];

int scount = 4;
int mod_type = 0;
unsigned char pmod_depth = 0x15;
unsigned char amod_depth = 0x00;


struct oscillator {
  char *wavetable;
  int index; // scaling factor 6
  int rate;  // also >> 6
};

oscillator osc[6];
oscillator lfo;

void setup() {
  cli();
  for (int i = 0; i < 256; i++) {
    // tri
    if (i < 128) triwave[i] = (char)(-128+2*i);
    else triwave[i] = (char)(127-2*i);
    // sin
    sinewave[i] = (char)(sin((i/256.0)*PI*2)*127.0);
    //saw
    sawwave[i] = (char)(i);
    //sqr
    if (i < 128) squarewave[i] = -127;
    else squarewave[i] = 127;
    
    if (i < 64)
      pulse[i] = 127;
    else
      pulse[i] = -127;
      
    noise[i] = random(-127, 127);
  } 

  // timers
  TCCR3A = 0x00;
  TCCR3B = 0x00;
  // timer 3 into fast pwm mode
  // TCCR3A = TCCR3A | B10000001;
  // TCCR3B = TCCR3B | B00001001;

  TCCR3A = (1 << COM3A1) | (1 << COM3A0) | (1 << WGM30);

  TCCR3B = (1 << CS30) | (1 << WGM32);
  OCR3AL = 0x00;
  OCR3AH = 0x00;
  TIMSK3 = 0x00;

  DDRE |= (1 << 3);

  // timer 4 to update OCR3AL
  TCCR4A = 0;//(1 << WGM42);
  OCR4A = 0xff;
  TCCR4B = (1 << CS41) | (1 << WGM42);
  TIMSK4 = (1 << OCIE4A);

  init_osc(&osc[0], sinewave, 440);
  init_osc(&osc[1], triwave, 440);
  init_osc(&osc[2], sawwave, 440);
  init_osc(&osc[3], squarewave, 440);
  init_osc(&osc[4], pulse, 440);
  init_osc(&osc[5], noise, 440);

  /* osc[0].rate = midi_lookup[60];
   osc[1].rate = midi_lookup[60+4];
   osc[2].rate = midi_lookup[60+7];//172<<2;
   osc[3].rate = midi_lookup[60+11];//220<<2;*/

  mod_type =  MOD_PITCH /*| MOD_AMP /*| MOD_XOR*/;

  init_osc(&lfo, triwave, 1);
  lfo.rate = 3;//midi_lookup[60];

  sei();

  // digitalWrite(13, HIGH);
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleControlChange(handleControlChange);
}
byte last_osc = 0;
byte osc_notes[] = {
  0,0,0,0,0,0};
// deals with midi notes
void handleNoteOn(byte chan, byte note, byte velocity)
{
  byte channel = chan-1;
  if (channel < 6)
  {
     osc_notes[channel] = note;
    osc[channel].rate = midi_lookup[note+24];
    set_osc_state(channel, ATTACK);
    osc_gains[channel] =0;
    last_osc = channel;
  }
  /*boolean found = false;
  // check if one is = 0
  for (int i = 0; i < 4; i++)
  {
    if (osc_notes[i] == 0)
    {
      osc_notes[i] = note;
      osc[i].rate = midi_lookup[note];
      set_osc_state(i, ATTACK);
      osc_gains[i] = 0;
      found = true;
      last_osc = i;
      break;
    } 
    else if (osc_notes[i] == note)
    {
      found = true;
      set_osc_state(i, ATTACK);
      osc_gains[i] = 0;
      break;
    }
  }

  if (!found)
  {
    last_osc = (last_osc+1)&3;
    osc[last_osc].rate = midi_lookup[note];
    osc_notes[last_osc] = note;
    set_osc_state(last_osc, ATTACK);
    osc_gains[last_osc] = 0;
  }*/
}

// deals with note offs
void handleNoteOff(byte channel, byte note, byte velocity)
{
  for (int i = 0; i < 6; i++)
    if (note == osc_notes[i])
    {
      //osc_notes[i] = 0;
      set_osc_state(i,RELEASE);
      break;
    }
}

// deals with control change
void handleControlChange(byte channel, byte number, byte value)
{
 /* switch (number)
  {
  case 0:
    lfo.rate = value*3;
    break;
  case 1:
    mod_type ^= MOD_PITCH;
    break;
  case 2:
    mod_type ^= MOD_AMP;
    break;
  case 3:
    mod_type ^= MOD_XOR;
    break;
  case 4:
    pmod_depth = value*2;
    break;
  case 5:
    amod_depth = value*2;
    break;
  case 6:
    lfo.wavetable = sinewave;
    break;
  case 7:
    lfo.wavetable = triwave;
    break;
  case 8:
    lfo.wavetable = sawwave;
    break;
  case 9:
    lfo.wavetable = squarewave;
    break;
  case 10:
    osc[0].wavetable = sinewave;
    osc[1].wavetable = sinewave;
    osc[2].wavetable = sinewave;
    osc[3].wavetable = sinewave;
    break;
  case 11:
    osc[0].wavetable = triwave;
    osc[1].wavetable = triwave;
    osc[2].wavetable = triwave;
    osc[3].wavetable = triwave;
    break;
  case 12:
    osc[0].wavetable = sawwave;
    osc[1].wavetable = sawwave;
    osc[2].wavetable = sawwave;
    osc[3].wavetable = sawwave;
    break;
  case 13:
    osc[0].wavetable = squarewave;
    osc[1].wavetable = squarewave;
    osc[2].wavetable = squarewave;
    osc[3].wavetable = squarewave;
    break;
  case 14: // attack time
    attack_time = value*40;
    break;
  
  case 15: //decay
    decay_time = value*40;
    break;
  
  case 16: // sustain
    sustain_level = value;
    break;
    
  case 17: // release
  release_time = value*40;
  break;

  case 123:
    for (int i = 0; i < 4; i++)
      {
        osc[i].rate = 0;
        osc_states[i] = OFF;
      }

  default:
    break; 
  }*/
  
  if (number == 9 && channel < 6)
  {
      set_osc_state(channel, RELEASE);
  }
}

// sets state, resets counter
void set_osc_state(byte which, byte state)
{
  osc_states[which] = state;
  osc_counters[which] = 0; 

  if (state == ATTACK)
  {
    osc_gains[which] = 0;
    osc_gainsteps[which] = (127<<8) / attack_time;
  }
  else if (state == RELEASE)
  {
    //osc_gains[which] = sustain_level << 8;
    osc_gainsteps[which] =  osc_gains[which] / release_time;
  }
  else if (state == DECAY)
  {
    //osc_gains[which] = 255 << 8;
    osc_gainsteps[which] = osc_gains[which] / decay_time; 
  }
}

ISR(TIMER4_COMPA_vect)
{
  static int count = 0;
  if (count == 2)
  {
    count = 0;
    static int temp = 0;
    temp = 0;

    char l = get_osc_sample(&lfo, 0);
    char offset = gain(l, pmod_depth);

    for (int i = 0; i < 6; i++) 
    {
      temp += gain(get_osc_sample(&osc[i], offset), get_osc_env_gain(i));

    }
    // when four
    temp >>= 2;
    // temp ^= l;
    if (mod_type & MOD_XOR) temp ^= l;
    if (mod_type & MOD_AMP) OCR3AL = 127-gain((char)temp, samp_to_factor(gain(l, amod_depth)));
    else OCR3AL = 127-temp;
  }
  count ++;
}

void loop() 
{
  /*for (byte i = 0; i < 4; i++)
  {
    osc[i].rate = midi_lookup[notes[random(0,7)]+random(1,3)*12];
    delay(200);
  }
  delay(3000);
*/
  // midi
   MIDI.read();
}

char gain(char input, byte factor) {
  if (factor == 0) return (char)0;

  int temp = input*factor;
  return (char)(temp >> 8); 
}

char get_osc_sample(struct oscillator* o, char offset) {
  if (!o->rate) return (char)0;
  char p_mod = 0;
  if (mod_type & MOD_PITCH) {
    p_mod = offset;
  }
  o->index += o->rate + p_mod;
  int i = o->index >> 6;
  if (i >= 256) {
    o->index = o->index - (256 << 6);
    i = o->index >> 6; 
  }

  return (o->wavetable[i%256]);
}
// gets the current gain of the oscillator depending on its state, incrememnting / decrementing various bits and pieces as necessary
unsigned char get_osc_env_gain( int osc )
{
  switch (osc_states[osc])
  {
  case OFF:
    return 0;

  case ATTACK:
    osc_counters[osc]++;
    osc_gains[osc] += osc_gainsteps[osc];
    if (osc_counters[osc] >= attack_time || osc_gains[osc] >= (254 << 8))
      set_osc_state(osc, DECAY);
    return osc_gains[osc]>>8;

  case DECAY:
    osc_counters[osc]++;
    osc_gains[osc] -= osc_gainsteps[osc];
    if (osc_counters[osc] >= decay_time || osc_gains[osc] < (sustain_level<<8))
      set_osc_state(osc, SUSTAIN);
    return osc_gains[osc]>>8;

  case SUSTAIN:
    return sustain_level;

  case RELEASE:
    osc_counters[osc]++;
    osc_gains[osc] -= osc_gainsteps[osc];
    if (osc_counters[osc] >= release_time || osc_gains[osc] < (1 << 8))
    {
      set_osc_state(osc, OFF);
     osc_notes[osc] = 0;
    return 0; 
    }
    return osc_gains[osc]>>8;
  } 
}


// interpolates (linearly) between neighbouring samples
char get_osc_sample_interp(struct oscillator* o, char offset) {
  if (!o->rate) return (char)0;
  char p_mod = 0;
  if (mod_type & MOD_PITCH) {
    p_mod = offset;
  }
  o->index += o->rate + p_mod;
  int i = o->index >> 6;
  if (i >= 256) {
    o->index = o->index - (256 << 6);
    i = o->index >> 6; 
  }

  int a = o->wavetable[i%256];
  int b = o->wavetable[(i+1)%256];
  a += (b-a)*(o->index&0x3f);

  return (char) a>>8;
}

void init_osc(struct oscillator* o, char *wavetable, int freq) {
  // do freq calcs
  static int l = 256 << 8;
  int f = freq << 8;
  //  o->rate = (l*freq)/15625;
  o->rate = 0;

  o->index = 0;
  o->wavetable = wavetable;
}

byte samp_to_factor(char samp) {
  int temp = 127-samp;
  return (byte)temp;
}

char biquad(char input) {

}  



