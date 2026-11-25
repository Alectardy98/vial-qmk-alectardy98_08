#!/usr/bin/env python3
"""Replay released F107 signals through the native calibration and scanner.

The fixture records last-active DAC values; the mocked comparator crosses one
count above them. Synthetic presses verify software paths, not physical travel.
"""
from pathlib import Path
import json
import subprocess
import tempfile
import unittest
import test_calibration as native

BOARD = native.BOARD / 'ibm/f107'
QUANTUM = native.QUANTUM.replace('#define MATRIX_COLS 16', '#define MATRIX_COLS 15')
QUANTUM = QUANTUM.replace(str(native.BOARD/'ibm/fat/config.h'), str(BOARD/'config.h'))
QUANTUM += '''
void keyboard_task(void);
#ifdef LEGACY
#undef LEYDEN_JAR_RANGE_CALIBRATION
#undef NB_CAL_BINS
#define NB_CAL_BINS 1
#undef NB_CUSTOM_CAL_BINS
#define NB_CUSTOM_CAL_BINS 0
#undef CUSTOM_CAL_BIN_KEYS
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {10}
#endif
'''
BASE_DRIVER = native.DRIVER[:native.DRIVER.index('int main(void)')]
DRIVER = BASE_DRIVER + r'''
void keyboard_task(void) { abort(); } // Startup calibration must never yield.
static const uint16_t captures[][18][8] = CAPTURES;
static const uint16_t held[][18][8] = HELD;
static const uint8_t held_positions[][2] = POSITIONS;
/* Each sample is {held row, col, neighbour row, col, held level, neighbour level}. */
static const uint16_t pairs[][6] = PAIRS;
int main(void) {
 eeconfig_init_kb();leyden_jar_init();unsigned old_faults=0;
 for(unsigned boot=0;boot<sizeof(captures)/sizeof(captures[0]);boot++) {
  memcpy(levels,captures[boot],sizeof(levels));leyden_jar_calibrate();
#ifndef LEGACY
  assert(NB_CAL_BINS==10 && NB_CUSTOM_CAL_BINS==5);
  assert(s_bin_map[11][7]==0 && s_bin_map[2][5]==1 && s_bin_map[6][6]==1 && s_bin_map[11][5]==1);
  assert(s_bin_map[13][5]==3 && s_bin_map[13][6]==3);
  assert(s_bin_map[4][5]==2 && s_bin_map[4][6]==2);
  assert(s_bin_map[7][2]==4 && s_bin_activation_offsets[4]==8);
  assert(s_bin_map[10][0]==4);
  for(unsigned b=0;b<NB_CAL_BINS;b++)assert(s_dac_thresholds[b]==s_dac_ref_level[b]+s_bin_activation_offsets[b]);
#endif
  for(unsigned sample=0;sample<sizeof(captures)/sizeof(captures[0]);sample++) {
   memcpy(levels,captures[sample],sizeof(levels));
   matrix_row_t matrix[8]={0};leyden_jar_logical_matrix_scan(matrix);
#ifdef LEGACY
   for(unsigned r=0;r<8;r++)old_faults+=__builtin_popcount(matrix[r]);
#else
   no_keys();
   for(unsigned c=0;c<18;c++)for(unsigned r=0;r<8;r++) {
    if(!leyden_jar_pad_is_active(r,c))continue;
    if(s_dac_thresholds[s_bin_map[c][r]]<levels[c][r]+5)fprintf(stderr,"margin boot %u sample %u ctrl %u,%u threshold %u level %u bin %u\n",boot,sample,r,c,s_dac_thresholds[s_bin_map[c][r]],levels[c][r],s_bin_map[c][r]);
    assert(s_dac_thresholds[s_bin_map[c][r]]>=levels[c][r]+5);
   }
#endif
  }
#ifndef LEGACY
  for(unsigned sample=0;sample<sizeof(held)/sizeof(held[0]);sample++) {
   memcpy(levels,held[sample],sizeof(levels));
   matrix_row_t matrix[8]={0};leyden_jar_logical_matrix_scan(matrix);
   unsigned row=7-held_positions[sample][0],col=held_positions[sample][1];
   for(unsigned r=0;r<8;r++) {
    unsigned expected=r==row?(1u<<col):0;
    if(matrix[r]!=expected) {
     fprintf(stderr,"held replay boot %u sample %u pressed %u,%u QMK row %u actual %x expected %x\n",boot,sample,held_positions[sample][0],col,r,matrix[r],expected);
     for(unsigned c=0;c<15;c++)if((matrix[r]^expected)&(1u<<c))fprintf(stderr," controller %u,%u value %u threshold %u bin %u\n",7-r,c,levels[c][7-r],s_dac_thresholds[s_bin_map[c][7-r]],s_bin_map[c][7-r]);
    }
    assert(matrix[r]==expected);
   }
  }
  for(unsigned sample=0;sample<sizeof(pairs)/sizeof(pairs[0]);sample++) {
   memcpy(levels,captures[boot],sizeof(levels));
   const uint16_t *pair=pairs[sample];
   levels[pair[1]][pair[0]]=pair[4];levels[pair[3]][pair[2]]=pair[5];
   matrix_row_t matrix[8]={0};leyden_jar_logical_matrix_scan(matrix);
   unsigned row=7-pair[0];
   for(unsigned r=0;r<8;r++)assert(matrix[r]==(r==row?(1u<<pair[1]):0));
  }
  /* Reject the full-sweep coupling bound with margin; allow an actual chord. */
  assert(s_dac_thresholds[3]>=448+5);
  assert(s_dac_thresholds[2]>=441+5);
  memcpy(levels,captures[boot],sizeof(levels));
  levels[13][5]=448;levels[13][6]=448;no_keys();
  levels[4][5]=441;levels[4][6]=434;no_keys();
  levels[13][5]=489;levels[13][6]=489;
  levels[4][5]=472;levels[4][6]=500;
  matrix_row_t chord[8]={0};leyden_jar_logical_matrix_scan(chord);
  for(unsigned r=0;r<8;r++)assert(chord[r]==((r==1 || r==2)?((1u<<13)|(1u<<4)):0));
  memcpy(levels,captures[boot],sizeof(levels));
  /* Conservative neighbouring-activity bounds from the earlier sweep. */
  levels[2][5]=433;levels[6][6]=447;levels[11][5]=445;levels[11][7]=422;
  no_keys();
  memcpy(levels,captures[boot],sizeof(levels));
  unsigned tested=0;
  for(unsigned r=0;r<8;r++)for(unsigned c=0;c<15;c++) {
   unsigned pr=s_matrixToControllerRow[r],pc=s_matrixToControllerCol[c];
   if(!leyden_jar_pad_is_active(pr,pc))continue;
   unsigned bin=s_bin_map[pc][pr];assert(bin<NB_CAL_BINS);
   uint16_t old=levels[pc][pr];levels[pc][pr]=s_dac_thresholds[bin];no_keys();
   levels[pc][pr]++;matrix_row_t matrix[8]={0};leyden_jar_logical_matrix_scan(matrix);
   for(unsigned row=0;row<8;row++)assert(matrix[row]==(row==r?(1u<<c):0));
   levels[pc][pr]=old;no_keys();tested++;
  }
  assert(tested==109); // Preserve the complete custom-layout pad union.
#endif
 }
#ifdef LEGACY
 assert(old_faults>0);printf("Original configuration reproduces %u false positions across released replays.\n",old_faults);
#else
 assert(writes==0);
 assert(leyden_jar_save_calibration_offset(1,34));assert(writes==1);
 leyden_jar_calibrate();assert(s_dac_thresholds[1]==s_dac_ref_level[1]+34);
 assert(leyden_jar_save_calibration_offset(1,34));assert(writes==1);
 assert(leyden_jar_restore_calibration_defaults());
 for(unsigned b=0;b<NB_CAL_BINS;b++)assert(s_dac_thresholds[b]==s_dac_ref_level[b]+s_bin_activation_offsets[b]);
 assert(leyden_jar_save_calibration_offset(0,18));
 leyden_jar_calibration_store_load(calibration_signature()^1,NB_CAL_BINS,s_bin_activation_offsets);
 assert(!leyden_jar_calibration_is_saved(0));
 puts("F107: six startup replays x six released captures, held signals, 93 coupling-pair samples and simultaneous chords, coupling bounds, 109 threshold boundaries per replay, persistence and defaults passed.");
#endif
}
'''


class F107Tests(unittest.TestCase):
    def test_live_capture_services_keyboard(self):
        values = json.loads((Path(__file__).parent/'fixtures/f107_recurrence.json').read_text())['released_sessions'][-1]['samples'][0]
        array = '{'+','.join('{'+','.join(str(values.get(f'{r},{c}',349)+1) for r in range(8))+'}' for c in range(18))+'}'
        source = BASE_DRIVER.replace('INITIAL', array) + r'''
static unsigned services, downs, ups;
static bool held_before, inject_press;
static uint16_t released_l;
void keyboard_task(void) {
 services++;
 assert(s_live_level_capture && s_calibration_ready);
 /* A macro attempting another capture cannot corrupt the active workspace. */
 if(services==1)assert(!leyden_jar_set_detect_levels());
 /* Replay last-active levels 426..428; the comparator crosses one above. */
 if(inject_press)levels[7][2]=(services>=3 && services<9)?427+(services%3):released_l;
 matrix_row_t m[8]={0};leyden_jar_logical_matrix_scan(m);
 bool held=m[5]&(1u<<7);
 if(held && !held_before)downs++;
 if(!held && held_before)ups++;
 held_before=held;
 for(unsigned r=0;r<8;r++)assert(m[r]==(r==5 && held?(1u<<7):0));
 /* The keyboard scanner reuses the PIO raw buffer and DAC between probes. */
 assert(threshold==s_dac_thresholds[NB_CAL_BINS-1]);
}
int main(void) {
 eeconfig_init_kb();leyden_jar_init();
 assert(!leyden_jar_set_detect_levels()); // Not calibrated yet.
 leyden_jar_calibrate();assert(services==0 && s_calibration_ready);
 assert(s_bin_map[7][2]==4 && s_bin_activation_offsets[4]==8);
 /* A released L at 409..411 and held L at 426..428 straddled the old
  * threshold of 427. Reproduce that lost press, then check the new margin. */
 uint16_t original_l=levels[7][2], new_threshold=s_dac_thresholds[4];
 assert(new_threshold<426 && new_threshold>=414);
 levels[7][2]=427;
 assert(leyden_jar_set_dac_threshold(427,4));no_keys();
 assert(leyden_jar_set_dac_threshold(new_threshold,4));
 for(unsigned level=427;level<=429;level++) {
  levels[7][2]=level;
  matrix_row_t m[8]={0};leyden_jar_logical_matrix_scan(m);
  for(unsigned r=0;r<8;r++)assert(m[r]==(r==5?(1u<<7):0));
 }
 for(unsigned level=410;level<=412;level++) {levels[7][2]=level;no_keys();}
 levels[7][2]=original_l;
 /* Left Arrow held 425..427 while its former threshold was 428. */
 assert(s_bin_map[10][0]==4);
 uint16_t original_left=levels[10][0];
 levels[10][0]=426;
 assert(leyden_jar_set_dac_threshold(428,4));no_keys();
 assert(leyden_jar_set_dac_threshold(new_threshold,4));
 for(unsigned level=426;level<=428;level++) {
  levels[10][0]=level;
  matrix_row_t m[8]={0};leyden_jar_logical_matrix_scan(m);
  for(unsigned r=0;r<8;r++)assert(m[r]==(r==7?(1u<<10):0));
 }
 for(unsigned level=409;level<=412;level++) {levels[10][0]=level;no_keys();}
 levels[10][0]=original_left;
 uint16_t references[NB_CAL_BINS],thresholds[NB_CAL_BINS];uint8_t bins[18][8];
 memcpy(references,s_dac_ref_level,sizeof(references));
 memcpy(thresholds,s_dac_thresholds,sizeof(thresholds));memcpy(bins,s_bin_map,sizeof(bins));
 assert(leyden_jar_set_detect_levels());assert(services>10 && services<150);
 for(unsigned c=0;c<18;c++)for(unsigned r=0;r<8;r++)assert(s_matrix_levels[c][r]==levels[c][r]-1);
 services=0;released_l=levels[7][2];inject_press=true;
 assert(leyden_jar_set_detect_levels());assert(downs==1 && ups==1 && !held_before);
 inject_press=false;services=0;assert(leyden_jar_set_detect_levels());
 for(unsigned c=0;c<18;c++)for(unsigned r=0;r<8;r++)assert(s_matrix_levels[c][r]==levels[c][r]-1);
 assert(!memcmp(references,s_dac_ref_level,sizeof(references)));
 assert(!memcmp(thresholds,s_dac_thresholds,sizeof(thresholds)));
 assert(!memcmp(bins,s_bin_map,sizeof(bins)));assert(writes==0);
 reject_dac=true;assert(!leyden_jar_set_detect_levels());reject_dac=false;
 assert(!s_live_level_capture && !s_calibration_ready && !leyden_jar_is_enabled());
 leyden_jar_calibrate();assert(s_calibration_ready);leyden_jar_enable(true);
 assert(leyden_jar_set_detect_levels());assert(!s_live_level_capture);
 puts("Live full-range measurement services normal scans, preserves calibration/EEPROM, rejects recursion and reports DAC failure.");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)
            (path/'quantum.h').write_text(QUANTUM)
            (path/'eeconfig.h').write_text(native.EEPROM)
            (path/'haptic.h').write_text('')
            (path/'test.c').write_text(source)
            subprocess.run(['cc','-std=gnu11','-g','-fsanitize=address,undefined','-I'+tmp,str(path/'test.c'),'-o',str(path/'test')],check=True)
            subprocess.run([str(path/'test')],check=True)

    def test_changed_baselines(self):
        fixture = json.loads((Path(__file__).parent/'fixtures/f107_recurrence.json').read_text())
        def array(values):
            return '{'+','.join('{'+','.join(str(values.get(f'{r},{c}',349)+1) for r in range(8))+'}' for c in range(18))+'}'
        sessions = fixture['released_sessions']
        sweep = fixture['sweep']
        source = BASE_DRIVER.replace('INITIAL', array(sessions[0]['samples'][0]))
        source += 'void keyboard_task(void) { abort(); }\n'
        source += 'int main(void) { eeconfig_init_kb(); leyden_jar_init();\n'
        for session in sessions:
            samples = session['samples']
            source += '{ const uint16_t released[][18][8] = {'+','.join(map(array,samples))+'};\n'
            source += '''
for(unsigned boot=0;boot<sizeof(released)/sizeof(released[0]);boot++) {
 memcpy(levels,released[boot],sizeof(levels));leyden_jar_calibrate();
 for(unsigned sample=0;sample<sizeof(released)/sizeof(released[0]);sample++) {
  memcpy(levels,released[sample],sizeof(levels));no_keys();
  for(unsigned c=0;c<18;c++)for(unsigned r=0;r<8;r++)if(leyden_jar_pad_is_active(r,c)) {
   assert(s_dac_thresholds[s_bin_map[c][r]]>=levels[c][r]+5);
  }
 }
 assert(s_dac_thresholds[2]<480); // 3/R must not inherit the numpad reference.
 assert(s_dac_thresholds[3]>s_dac_thresholds[2]+20);
}}
'''
        source += '{ const uint16_t base[18][8] = '+array(sweep['baseline_lower_bounds'])+';\n'
        source += 'memcpy(levels,base,sizeof(levels));leyden_jar_calibrate();no_keys();\n'
        for key,peak in sweep['dominant_peak_lower_bounds'].items():
            r,c=map(int,key.split(','))
            source += f'assert(s_dac_thresholds[s_bin_map[{c}][{r}]]<={peak});\n'
        for sample in sweep['pairs']:
            hr,hc=map(int,sample['held'].split(','));nr,nc=map(int,sample['neighbour'].split(','))
            source += f'''memcpy(levels,base,sizeof(levels));
levels[{hc}][{hr}]={sample['held_level']+1};levels[{nc}][{nr}]={sample['neighbour_level']+1};
{{matrix_row_t m[8]={{0}};leyden_jar_logical_matrix_scan(m);
for(unsigned r=0;r<8;r++)assert(m[r]==(r=={7-hr}?(1u<<{hc}):0));}}
'''
        source += '}\n'
        source += '{const uint16_t now[18][8]='+array(sessions[-1]['samples'][0])+';\n'
        source += '''memcpy(levels,now,sizeof(levels));leyden_jar_calibrate();
levels[14][5]=512;matrix_row_t m[8]={0};leyden_jar_logical_matrix_scan(m);
for(unsigned r=0;r<8;r++)assert(m[r]==(r==2?(1u<<14):0));
memcpy(levels,now,sizeof(levels));no_keys();}
/* Identical baselines must still produce nonempty bins and assign every pad.
 * The full DAC endpoint must saturate, never wrap into false typing. */
for(unsigned level=400;level<=1024;level+=624) {
 for(unsigned c=0;c<18;c++)for(unsigned r=0;r<8;r++)levels[c][r]=level;
 leyden_jar_calibrate();unsigned counts[NB_CAL_BINS]={0};
 for(unsigned c=0;c<18;c++)for(unsigned r=0;r<8;r++)if(leyden_jar_pad_is_active(r,c)) {
  assert(s_bin_map[c][r]<NB_CAL_BINS);counts[s_bin_map[c][r]]++;
 }
 for(unsigned b=0;b<NB_CAL_BINS;b++) {
  assert(counts[b]>0);assert(s_dac_thresholds[b]<=1023);
  if(level==1024)assert(s_dac_thresholds[b]==1023);
 }
 if(level==400)no_keys();
}
assert(writes==0);
puts("Changed-baseline sessions, 103 sweep responses, current coupling, numpad +, ties and DAC saturation passed.");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)
            (path/'quantum.h').write_text(QUANTUM)
            (path/'eeconfig.h').write_text(native.EEPROM)
            (path/'haptic.h').write_text('')
            (path/'test.c').write_text(source)
            subprocess.run(['cc','-std=gnu11','-g','-fsanitize=address,undefined','-I'+tmp,str(path/'test.c'),'-o',str(path/'test')],check=True)
            subprocess.run([str(path/'test')],check=True)

    def run_case(self, legacy):
        samples = json.loads((Path(__file__).parent/'fixtures/f107_released.json').read_text())['samples']
        cases=json.loads((Path(__file__).parent/'fixtures/f107_pressed.json').read_text())['cases']
        pairs=json.loads((Path(__file__).parent/'fixtures/f107_coupling_pairs.json').read_text())['cases']
        def array(values):
            return '{'+','.join('{'+','.join(str(values.get(f'{r},{c}',349)+1) for r in range(8))+'}' for c in range(18))+'}'
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)
            (path/'quantum.h').write_text(QUANTUM)
            (path/'eeconfig.h').write_text(native.EEPROM)
            (path/'haptic.h').write_text('')
            pair_rows=[]
            for case in pairs:
                held=','.join(map(str,case['held_controller']))
                neighbour=next(k for k in case['levels'] if k!=held)
                values=case['held_controller']+list(map(int,neighbour.split(',')))
                values += [case['levels'][held]+1,case['levels'][neighbour]+1]
                pair_rows.append('{'+','.join(map(str,values))+'}')
            source=DRIVER.replace('INITIAL',array(samples[0]))
            source=source.replace('CAPTURES','{'+','.join(map(array,samples))+'}')
            source=source.replace('HELD','{'+','.join(array(c['levels']) for c in cases)+'}')
            source=source.replace('POSITIONS','{'+','.join('{'+','.join(map(str,c['held_controller']))+'}' for c in cases)+'}')
            source=source.replace('PAIRS','{'+','.join(pair_rows)+'}')
            (path/'test.c').write_text(source)
            cmd=['cc','-std=gnu11','-g','-fsanitize=address,undefined','-I'+tmp,str(path/'test.c'),'-o',str(path/'test')]
            if legacy:cmd.insert(1,'-DLEGACY')
            subprocess.run(cmd,check=True)
            subprocess.run([str(path/'test')],check=True)

    def test_original_false_presses(self):self.run_case(True)
    def test_released_noise_and_press_paths(self):self.run_case(False)

if __name__=='__main__':unittest.main(verbosity=2)
