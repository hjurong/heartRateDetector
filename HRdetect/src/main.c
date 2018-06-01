/*
 * main.c
 *
 *  Created on: 17/05/2015
 *      Author: JURONG
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL

/*
 * global variable declaration //////////////////////////////////////////////////////////
 * 
 * done: a flag to indicate whether the current ADC conversion is done
 * switched: a flag to indicate if an input has exceeded the THRESHOLD
 * time_changed: a flag to indicate time update, which occurs every second
 * reset_counter: a flag raised when the timer1 counter is reset (occurs every second)
 *
 * min: number of minutes passed
 * sec: number seconds passed
 *
 * */
volatile static uint8_t done = 0;
volatile static uint8_t switched = 0;
volatile static uint8_t time_changed = 0;
volatile static int reset_counter = 0;
volatile static int min = 0, sec = 0;

/*
 * function declarations /////////////////////////////////////////////////////////////////
 *
 * */
void cal_stats(int *NN_diff_mean, int *N_beats,
			   uint16_t *time2cnt, uint16_t *end_edge,
			   int *NN_mean, int *NN_current, int32_t *SDNN, int *RMSSD,
			   int32_t *SDSD, int *NN50, int *beat_cnt, int *num_resets);
void moving_average(uint16_t *num_data, int *moving_avg,
					uint8_t *window_filled, int *delta, int window[]);
void find_THRESHOLD(uint8_t *found_THRES, int *THRESHOLD,
					uint16_t *num_data, int *moving_avg, int PRE_DATA);
void send_8bit_nibble(uint8_t RS, uint8_t data);
void init_timers();
void init_LCD();
void init_ADC();
void int2str2(int *STAT, uint8_t out_buffer[]);
void displayStats(int *MNN, int32_t *SDNN, int *RMSSD,
				  int32_t *SDSD, int *N_beats, int *NN50);
void debug_display(int *data1, int *data2, int *data3, 
                   int *data4, int *data5, int *data6);


/*
 * main //////////////////////////////////////////////////////////////////////////////////
 * */
int main(void) {


	/*
	 * initialise parameters
	 *
	 * type == int ==> signed 16-bit int ==> min = -2^15 and max = 2^15 = 32768
     * type int is used as stat containers so that negative arithematics 
     * can be performed 
     * (this implies that these values would overflow at 32768
     * ==> need to take extract care ensuring that this does not happen)
     *
	 * type == int32_t ==> signed 32-bit int ==> min = -2^31 and max = 2^31
     * type int32_t is used for the two STANDARD DEVIATION stats to avoid 
     * undesired overflow
     *
	 * */
	int NN_current = 0; // current NN interval; unit: ms
	int N_beats = 0; // total number of HB since the start
	int beat_cnt = 0; // number of HB in every 15 seconds
	int NN_mean = 0; // mean of RR-interval durations; reset every 15 seconds
	int NN_diff_mean = 0; // mean of RR-interval difference; reset every 15 seconds
	int RMSSD = 0; // root-mean-sqaure of RR-interval difference
	int NN50 = 0; // number of adjacent NN-intervals > 50ms
    
	int32_t SDNN = 0; // standard deviation of ALL NN intervals
	int32_t SDSD = 0; // std(NN_{i} - NN_{i-1})
    
    /*
     * initialise params for the computation of THRESHOLD and low-pass filtering
     * by moving average
     *
     * type == int ==> negative numbers can be handled
     *
     * */
	int THRESHOLD = 836; // default THRESHOLD
	int window[20]; // window on which moving average is performed
	int moving_avg = 0xfff; // current value of filtered signal
	int mdelta = 0; // sum of all values within the window
    
    /*
     * initialise additional counters and flags
     * */
    // uint16_t == unsigned 16-bit int ==> range = [0, 2^16]
    // this is used instead of int as more counts can be stored
	uint16_t time2cnt = 0; // counter: timer1 counter value when a beat is detected; 
	uint16_t num_data = 0; // counter: number of data processed

	uint8_t found_THRES = 0; // flag: THRESHOLD is found
	uint8_t window_filled = 0; // flag: WINDOW is filled


	/*
	 * initalise hardware
	 *
	 * */

	asm("sei"); // enable interrupts
	init_LCD();
	init_ADC();
	init_timers();

	// enable debug LEDs
	DDRB = 0b00000011; // set PB0 and PB1 as output
	PORTB = 0b00000011; // initialise both LEDs to be OFF


	while (1) {

		if (done) {
			/*
			 * if current ADC conversion is done
			 * */
			num_data += 1; // count the number of data converted

			/*
			 * perform moving average filtering
			 * */
			moving_average(&num_data, &moving_avg, &window_filled, &mdelta, window);

			/*
			 * use the first 2000 conversions to set an appropriate THRESHOLD;
			 * then start doing calculations
			 *
             * 2000 data is selected because the slowest signal is 40-beats/min ==> 
             * 1 beat/1.5sec; and since the sampling rate of the system is set to 
             * 1.02 kHz ==> 1020 samples/sec ==> 2000 samples is equivalent to 
             * approximately 2 seconds ==> the MINIMUM of the signal can be detected
             *
			 * */
			if (num_data <= 2000 && !found_THRES) {
				find_THRESHOLD(&found_THRES, &THRESHOLD, &num_data, &moving_avg, 2000);
			}

			else {
				if ((moving_avg < THRESHOLD)) {
                    /*
                     * a beat is detected if the moving average is below the THRESHOLD
                     * i.e. the S-peak, which follows the R-peak immediately is used
                     * to detect the R-R interval
                     * */
					if ((switched == 0)) {
                        /*
                         * if all previous values are above the THRESHOLD, register as 
                         * a beat
                         **/
                        
                        // capture current time immediately
						uint16_t end_edge = TCNT1;
						int num_cnt_reset = reset_counter;
						reset_counter = 0; // clear flag
						
                        
                        // debug //////////////////////////////////////////////////////
                        // SET time_changed FLAG TO 0 IN TIMER1 INTERRUPT ROUTINE 
                        // BEFORE ENABLING DEBUG DISPLAY
                        ///////////////////////////////////////////////////////////////
						//int ed = end_edge;
						//int ss = sec;
						//debug_display(&NN_current, &num_cnt_reset, &time2cnt,
                        //              &NN_mean, &beat_cnt, &RMSSD); // debug
                        
                        
                        // calculate stats using the moving average
						cal_stats(&NN_diff_mean, &N_beats, &time2cnt, &end_edge,
								  &NN_mean, &NN_current, &SDNN,
								  &RMSSD, &SDSD, &NN50, &beat_cnt, 
                                  &num_cnt_reset);
								  
						switched = 1; // prevent detecting multiple peaks

					}

                    // debug
					PORTB = 0b00000001; // turn GREEN LED ON if a beat is detected

				}

				else {

					switched = 0; // reset switched flag; and wait for next peak
					PORTB = 0b00000011; // turn GREEN LED OFF
                    
				}

			}

			done = 0; // once computations are done, reset flag

		} // end of if (done)

		/*
		 * update LCD display when time_changed flag is raised, which
		 * is EXACTLY every second
		 * */
		if (time_changed) { // && (N_beats > 2)
            
            // display the stats;
			displayStats(&NN_mean, &SDNN, &RMSSD,
						 &SDSD, &N_beats, &NN50);

			time_changed = 0; // reset flag

			if (sec % 15 == 0) {
				/*
				 * reset the means every 15 seconds
				 * */
				beat_cnt = 0;
				NN_mean = 0; // mean of RR-interval durations
				NN_diff_mean = 0; // mean of RR-interval difference

			}
			
			if (min % 5 == 0) {
				/*
				 * reset the standard deviations every 5 mins to avoid overflow
				 * */
				SDNN = 0;
				SDSD = 0;
			}
			

		}

	} // end of while (1)

}


/*
 * interrupt routines //////////////////////////////////////////////////////////////
 *
 * */
ISR(TIMER1_COMPA_vect) {
	/*
	 * timer1 output compare interrupt routine; for time keeping,
	 * this routine is called every second as the compare value for timer1 is
	 * set to be 15625
	 *
	 * ==> 1sec = 1/(16MHz) * 1024 * cnt ==> cnt = 15625 ==> increment global sec
	 *
	 * if sec % 60 == 0: increment min, set sec == 0
	 *
	 * */

	sec = (sec+1) % 60; // increment second

	time_changed = 1; // raise time_changed flag; trigger LCD display update
	reset_counter += 1; // increment timer reset flag to ensure correct time keeping
	TCNT1 = 0; // reset timer 1 counter

	if (sec == 0) {

		min = (min+1); // increment min every 60 sec
	}

}

ISR(ADC_vect) {
	/*
	 * ADC single conversion interrupt routine
	 *
	 * raise done flag so main() can start reading the converted value;
	 * the start conversion events are controlled by timer0;
     * this ensures precise sampling rate
	 *
	 * */
	done = 1; // raise done flag ==> start processing in main()
}


ISR (TIMER0_OVF_vect)  {
	/*
     * timer0 overflow interrupt, event to be executed every 0.98ms
     * 
     * nn(ms) = 1/(16MHz) * 64 * 245 --- i.e. prescaler = 64, count = 245
     * 0.98ms is chosen to avoid racing condition between this interrupt and 
     * that of timer1
     *
     * */ 
	ADCSRA |= (1 << ADSC); // start the ADC conversion
	TCNT0 = 10; // reset timer0 count

}


/*
 * function implementations ///////////////////////////////////////////////////////
 *
 * */

void find_THRESHOLD(uint8_t *found_THRES, int *THRESHOLD,
					uint16_t *num_data, int *moving_avg, int PRE_DATA) {
	/*
	 * use the first N (i.e. PRE_DATA) data to determine the appropriate
	 * THRESHOLD which is used to detect the S peaks
	 *
	 * */
	if (*moving_avg < *THRESHOLD) {
		/* code
		 *
		 * the negative peaks have more stable amplitude;
		 * therefore, they are used to register the RR-intervals
		 *
		 * */
		*THRESHOLD = *moving_avg;
	}

	if (*num_data == PRE_DATA) {
		/* code
		 *
		 * at the Nth point, update THRESHOLD and raise found_THRES flag
		 * so this condition will not be entered again even if the 16 bit
		 * counter "num_data" overflows
		 *
		 * */
		*THRESHOLD += 40; // every increment == 0.0049V; thus: +40 ==> +0.196V
		*found_THRES = 1;
	}
}

void moving_average(uint16_t *num_data, int *moving_avg,
					uint8_t *window_filled, int *delta, int window[]) {
	/*
	 * compute the moving average of the signal; this acts as a lowpass filter
	 *
	 *             moving_average = input_signal "*" window;
     * where "*" denotes CONVOLUTION
     *
     * by definition, CONVOLUTION is computed by:
     *
     *             moving_avg = \sum_{j=-q}^{q} b_j * y_{t+j} ... q < t < N-q
     *
     * where:
     * window_len = 2*q
     * y_k = values contained in window
	 * */

	uint16_t ADCval = ADCW; // read 10-bit data
	int val = (int)ADCval; // convert to int as computations are performed with int

	if (*num_data <= 20 && !*window_filled) {
		/* code
		 *
		 * if there are less than 20 values being converted, which is less
		 * than the window length, moving average cannot be calculated yet;
		 * simply fill the window buffer and sum the values
		 *
		 * */
		uint8_t index = *num_data % 20; // find where to put the current data
		window[index] = val; // put data in window
		*delta += val; // calculate the sum 

	}
    
	else {
		/*
		 * once there are enough data start computing the moving average:
		 *
		 * moving_avg = \sum_{j=-q}^{q} b_j * y_{t+j} ... q < t < N-q
		 *
		 * where:
		 * window_len = 2*q
		 * y_k = values contained in window
		 * */

		// raise flag so the previous condition will not be entered
		// again even if overflow of num_data occurs
		*window_filled = 1;
		// find the appropriate index to place current data
		uint8_t index = *num_data % 20;
        // the window is essentially a stack of length 20; to compute
        // the moving sum:
        //          window_sum = window_sum + latest_value - oldest_value
		*delta = *delta + val - window[index];
		
		window[index] = val; // update the buffer
		*moving_avg = *delta / 20; // moving average; b_j = 1/20
	}
}


void cal_stats(int *NN_diff_mean,
			   int *N_beats, uint16_t *time2cnt, uint16_t *end_edge,
			   int *NN_mean, int *NN_current,
			   int32_t *SDNN, int *RMSSD,
			   int32_t *SDSD, int *NN50, int *beat_cnt, int *num_resets) {
	/*
	 * compute the relevant statistics
     *
     * end_edge: uint16_t; timer1 counter value when a beat is detected
     * num_resets: int; number of timer1 counter resets since last detected beat
     * time2cnt: uint16_t; timer1 counter value of previous beat
     *
	 * */

	/*
	 * if threshold is exceeded but has not switched, this is
	 * registered as a beat;
	 *
	 * ==> increment N_beats
	 *
	 * */
    
	*N_beats += 1;

//	uint16_t time1cnt = 0; // local; temp container
//	int NN_prev = 0; // local
//	int NN_diff = 0; // local

	if (*N_beats < 2) {
		/*
		 * if this is the first beat, cannot calculate RR-interval as
		 * two beats are required; simply record the current time
		 * (i.e. number clock cycles) from timer 1
		 *
		 * since the prescaler is set to 1024 ==> every 16 increments
		 * of TCNT1 is equivalent to approximately 1ms:
		 *
		 * 1ms = 1 / (16MHz) * 1024 * cnt ==> cnt = 15.5 ~~ 16
		 *
		 * ==> the unit of time2cnt & time1cnt are in ms
         * ==> subsequent stats are also in ms
		 * */
		
		*time2cnt = *end_edge / 16; // convert to ms
	}

	else {
		/* if more than 2 beats have occurred:
		 *
		 * -->store old time2cnt in time1cnt, which represents a length 2 stack
		 * -->read the current count from timer 1 (need to beware of potential
		 *    counter reset) and push into time2cnt
		 * */

		int time1cnt = *time2cnt; // unit: ms
		*time2cnt = *end_edge / 16; // convert to ms

		if (*N_beats < 3) {
			/*
			 * if there is only 1 RR-interval; i.e. 2 beats
			 * cannot calculate NN_diff
			 * 
             * but the stats for current interval can be computed
			 * */
			
			int t2 = *time2cnt; // time2cnt: uint16_t --> int
			*NN_current = (1000 * (*num_resets) + t2 - time1cnt); // unit: ms

			/*
			 * online variance && mean using the Knuth algorithm
             * integer division is used ==> loss of precision
             *
			 * */
            
			int delta = *NN_current - *NN_mean; // unit: ms
			*NN_mean = *NN_mean + (delta / (*beat_cnt)); 
			*SDNN += delta*(*NN_current - *NN_mean);
            
		}

		else {
			/*
			 * if there are more than 2 RR-intervals, calculate ALL stats
			 *
			 * */
            
			*beat_cnt += 1; // increment beat count
			int NN_prev = *NN_current; // store previous NN interval length in container
            
            // convert from uint16_t --> int; this step is required; otherwise the next
            // computation would mix uint16_t and int arithmetics, which can result
            // in unexpected behaviour
            // since timer1 counter is reset every second ==> 1 reset == 1000ms
			int t2 = *time2cnt; 
			*NN_current = (1000 * (*num_resets) + t2 - time1cnt); // unit: ms
			
            
            // debug //////////////////////////////////////////////////////
            // SET time_changed FLAG TO 0 IN TIMER1 INTERRUPT ROUTINE 
            // BEFORE ENABLING DEBUG DISPLAY
            ///////////////////////////////////////////////////////////////
			//debug_display(&t2, &time1cnt, num_resets, 
            //              NN_diff_mean, num_resets, num_resets);
            

			/*
			 * compute RMSSD:
			 *
			 * sqrt(sum_{i}{NN_diff^2} / (N_beats-1))
			 *
			 * where: NN_diff = NN_{i}-NN_{i-1}
			 *
			 * RMSSD = sum_{i}{NN_diff^2} ==>
			 * need to divide by (N_beats-2-1) then take sqrt when outputting data
			 *
			 * */

			int NN_diff = abs(*NN_current - NN_prev); // unit: ms
			int delta = (NN_diff * NN_diff) - *RMSSD;
			*RMSSD += delta / (*N_beats-2); // unit: ms^2

			// compute variance; i.e. SDSD
			// need to divide SDSD by (N_beats-2-1) then take sqrt to 
            // get standard deviation
			delta = NN_diff - *NN_diff_mean;
			*NN_diff_mean += delta / (*beat_cnt); // unit: ms
			*SDSD += delta*(NN_diff - *NN_diff_mean); // unit: ms

			if (NN_diff > 50) {
				/*
				 * if the difference between two intervals are > 50ms,
				 * increment NN50
				 *
				 * RECALL: NN_diff has unit ms
				 * */

				*NN50 += 1;
			}

			/*
			 * online variance && mean (Knuth algorithm)
             * integer division ==> loss in precision
			 * */
			delta = *NN_current - *NN_mean;
			*NN_mean = *NN_mean + (delta / (*beat_cnt)); // unit: ms
			*SDNN += delta*(*NN_current - *NN_mean); // unit: ms^2
            
		}
	}
}


void send_8bit_nibble(uint8_t RS, uint8_t data) {
	/*
	 * send 8-bit data to LCD display
	 *
     * RS == 0 && EN == 1 ==> send command
     * RS == 1 && EN == 1 ==> send message
     * RS == * && EN == 0 ==> execute
	 * */

	PORTC = (RS << 4) | (1 << 5); // set RS and EN==1
	_delay_ms(2);

	PORTD = data; // send data through PORTD
	_delay_ms(2);

	PORTC = (RS << 4) | (0 << 5); // enable is LOW ==> execute
	_delay_ms(2);

}



void init_timers() {
	/*
	 * Timer1 amd Timer0 initialisation
     *
	 * Timer1: keeping global time; i.e. incrementing sec&min, NN_intervals etc.
     * Timer0: generate precise timing for ADC conversion ==> controls sampling rate
	 * */
    
    // timer1 //////////////////////////////////////////////////////
	OCR1A = 15625; // ==> 1 sec; refer to timer1 interrupt routine

	// set timer 1 Mode 4, CTC on OCR1A
	TCCR1B |= (1 << WGM12);

	// set interrupt on compare match
	TIMSK |= (1 << OCIE1A);

	// set pre-scaler to 1024 ==> 00000101
	TCCR1B |= (1 << CS12) | (1 << CS10);

    // timer0 //////////////////////////////////////////////////////
    TCCR0 = (1 << CS01) | (1 << CS00); // prescaler of 64
	TCNT0 = 10; // start ADC conversion every 0.98ms
	TIMSK |= (1 << TOIE0);

}

void init_LCD() {
	/*
	 * DDRC7: N/A
	 * DDRC6: N/A
	 * DDRC5: Enable
	 * DDRC4: Register Select
	 * DDRC3: N/A
	 * DDRC2: N/A
	 * DDRC1: N/A
	 * DDRC0: N/A
	 *
	 * */
	DDRC = 0b00110000;

	/*
	 * DDRD == 8 bit output; order REVERSED
	 *
	 * ==> 8-bit nibble can be sent to LCD at once
	 *
     * DDRD7: bit0 data
	 * DDRD6: bit1 data
	 * DDRD5: bit2 data
	 * DDRD4: bit3 data
	 * DDRD3: bit4 data
	 * DDRD2: bit5 data
	 * DDRD1: bit6 data
	 * DDRD0: bit7 data
     *
	 * */
    
	DDRD = 0xff; // set ALL ports as output

	_delay_ms(15);
	send_8bit_nibble(0, 0b00011100); // set 5x7 dot format, 2 line mode, 8-bit data
	_delay_ms(5);

	send_8bit_nibble(0, 0b11110000); // Display ON, Cursor ON, Cursor Blinking

	send_8bit_nibble(0, 0b01100000); // character entry mode increment/display shift off

	send_8bit_nibble(0, 0b10000000); // clear screen
	send_8bit_nibble(0, 0b01000000); // take display cursor home
	_delay_ms(15);
}


void init_ADC() {
	/*
	 * ADC initialisation
	 *
     * sysclk: 16MHz
     * prescaler: 128
     * conversion time: 13 clock cycles
     *
     * ==> ADC_clk_freq = 16MHz / 128 = 125kHz ---> 50kHz < 125kHz < 200kHz
     * ==> ADC can operate at full resolution
     *
	 * */

	ADMUX = 0; // use ADC0
	ADMUX &= ~(1 << ADLAR); // 10-bit resolution
	ADMUX |= (1 << REFS0); // use AVcc as reference voltage
	ADMUX &= ~( 1 << REFS1);

	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // prescaler = 128
	ADCSRA |= (1 << ADEN); // enable ADC
	ADCSRA |= (1 << ADIE); // enable ADC interrupt

	ADCSRA |= (1 << ADSC); // start the ADC conversion
}

void int2str2(int *STAT, uint8_t out_buffer[]) {
	/*
	 * convert a 16-bit int to an array of char; then map each
	 * char to the appropriate output to the LCD
	 *
     * using itoa() in <stdlib.h>
	 * */
    
	uint8_t len = 6; // 16-bit int ==> max == 0xFFFF = 65535 ==> max 6 digits
	char buffer[len];
	itoa(*STAT, buffer, 10); // int --> char[]

	uint8_t i;
	for (i = 0; i < len; i++) {
		switch(buffer[i]) {
		case '0':
			out_buffer[i] = 0b00001100; // ORDER REVERSED
			break;
		case '1':
			out_buffer[i] = 0b10001100;
			break;
		case '2':
			out_buffer[i] = 0b01001100;
			break;
		case '3':
			out_buffer[i] = 0b11001100;
			break;
		case '4':
			out_buffer[i] = 0b00101100;
			break;
		case '5':
			out_buffer[i] = 0b10101100;
			break;
		case '6':
			out_buffer[i] = 0b01101100;
			break;
		case '7':
			out_buffer[i] = 0b11101100;
			break;
		case '8':
			out_buffer[i] = 0b00011100;
			break;
		case '9':
			out_buffer[i] = 0b10011100;
			break;
		case '-':
			out_buffer[i] = 0b00001100; // 0b10110100; // char "-"
			break;
		default:
			out_buffer[i] = 0b01111111;
			break;
		}
	}

}


void displayStats(int *MNN, int32_t *SDNN, int *RMSSD,
				  int32_t *SDSD, int *N_beats, int *NN50) {

	/*
	 * send message to LCD:
	 *
	 * mm:ss Bxxxx Hyyy -- (16 char)
	 *  			    -- (always)
	 *
	 * 				    -- (alternating, every 5 seconds)
	 * MNN:yyyy SDN:yyy -- (16 char) unit: ms
	 * RSD:yyy  N50:yyy -- (16 char) unit: ms
	 * SDD:yyy  pNN:yyy%-- (16 char) unit: ms
	 *
	 * data is represented in REVERSE order;
	 * e.g. on data sheet: 0011 0000 --> "0"; but need to send: 0000 1100
	 *
	 * */

	send_8bit_nibble(0, 0b10000000); // clear screen
	send_8bit_nibble(0, 0b01000000); // take display cursor home
	_delay_ms(15);

	/*
	 * firstly display time info: MM:SS
	 * */

	uint8_t out_buffer[6]; // output mapping container
	int temp_data;

	temp_data = min; // convert minutes
	int2str2(&temp_data, out_buffer);
	send_8bit_nibble(1, out_buffer[0]); // only interested in the first two digits
	send_8bit_nibble(1, out_buffer[1]);

	send_8bit_nibble(1, 0b01011100); // char: ":"

	temp_data = sec;
	int2str2(&temp_data, out_buffer); // reuse buffer
	send_8bit_nibble(1, out_buffer[0]); // only interested in the first two digits
	send_8bit_nibble(1, out_buffer[1]);

	/*
	 * send number of beats and heart rate info: B:yyyy R:yyy
	 *
	 * */
	send_8bit_nibble(1, 0b01111111); // char "space"
	send_8bit_nibble(1, 0b01000010); // char "B"

	int2str2(N_beats, out_buffer); // number of beats since beginning
	send_8bit_nibble(1, out_buffer[0]); // only interested in the first 4 digits
	send_8bit_nibble(1, out_buffer[1]);
	send_8bit_nibble(1, out_buffer[2]);
	send_8bit_nibble(1, out_buffer[3]);

	send_8bit_nibble(1, 0b01111111); // char "space"
	send_8bit_nibble(1, 0b01001010); // char "R"
	
	int MNN_val = *MNN;
	if (MNN_val < 1) {
		MNN_val = 30002; // avoid initial division by zero
	}
	
	temp_data = (30000 / MNN_val) * 2; // HR; unit: beats/min; 1000 * 30 * 2 / *MNN
	int2str2(&temp_data, out_buffer);
	send_8bit_nibble(1, out_buffer[0]); // only interested in the first 3 digits
	send_8bit_nibble(1, out_buffer[1]);
	send_8bit_nibble(1, out_buffer[2]);

	// switch to second line
	send_8bit_nibble(0, 0b00000011);
    
    // alternate stats every 5 seconds
	if ((sec/5)%3 == 0) {
		/*
		 * dsiplay: MNN:yyyy SDN:yyy
		 *
		 * */

		send_8bit_nibble(1, 0b10110010); // char "M"
		send_8bit_nibble(1, 0b01110010); // char "N"
		send_8bit_nibble(1, 0b01110010); // char "N"
		send_8bit_nibble(1, 0b01011100); // char ":"

		// MNN unit: ms
		int2str2(MNN, out_buffer); // convert MNN to string; unit: ms
		send_8bit_nibble(1, out_buffer[0]); // interested in the first 4 digits
		send_8bit_nibble(1, out_buffer[1]); // read from most significant bit
		send_8bit_nibble(1, out_buffer[2]);
		send_8bit_nibble(1, out_buffer[3]);

		send_8bit_nibble(1, 0b01111111); // char "space"
		send_8bit_nibble(1, 0b11001010); // char "S"
		send_8bit_nibble(1, 0b00100010); // char "D"
		send_8bit_nibble(1, 0b01110010); // char "N"
		send_8bit_nibble(1, 0b01011100); // char ":"

		temp_data = sqrt(abs(*SDNN / (*N_beats - 2))); // SDNN; unit: ms
		int2str2(&temp_data, out_buffer); // convert SDNN to string
		send_8bit_nibble(1, out_buffer[0]); // only interested in the first 3 digits
		send_8bit_nibble(1, out_buffer[1]);
		send_8bit_nibble(1, out_buffer[2]);

	}

	else if ((sec/5)%3 == 1) {
		/*
		 * display RSD:yyy N50:yyy
		 *
		 * */

		send_8bit_nibble(1, 0b01001010); // char "R"
		send_8bit_nibble(1, 0b11001010); // char "S"
		send_8bit_nibble(1, 0b00100010); // char "D"
		send_8bit_nibble(1, 0b01011100); // char: ":"
        
        temp_data = sqrt(abs(*RMSSD)); // RMSSD; unit: ms
		int2str2(&temp_data, out_buffer); // convert RMSSD to string
		send_8bit_nibble(1, out_buffer[0]); // only interested in the first 3 digits
		send_8bit_nibble(1, out_buffer[1]); // read from most significant bit
		send_8bit_nibble(1, out_buffer[2]);

		send_8bit_nibble(1, 0b01111111); // char "space"
		send_8bit_nibble(1, 0b01110010); // char "N"
		send_8bit_nibble(1, 0b10101100); // char "5"
		send_8bit_nibble(1, 0b00001100); // char "0"
		send_8bit_nibble(1, 0b01011100); // char: ":"
		
		int2str2(NN50, out_buffer); // convert NN50 to string
		send_8bit_nibble(1, out_buffer[0]); // only interested in the first 3 digits
		send_8bit_nibble(1, out_buffer[1]);
		send_8bit_nibble(1, out_buffer[2]);
	}

	else if ((sec/5)%3 == 2) {
		/*
		 * display SDD:yyy pNN:yyy
		 */
        
		send_8bit_nibble(1, 0b11001010); // char "S"
		send_8bit_nibble(1, 0b00100010); // char "D"
		send_8bit_nibble(1, 0b00100010); // char "D"
		send_8bit_nibble(1, 0b01011100); // char: ":"

		temp_data = sqrt(abs(*SDSD / (*N_beats-2))); // unit: number of clock cycles
		int2str2(&temp_data, out_buffer); // convert MNN data to string
		send_8bit_nibble(1, out_buffer[0]); // only interested in the first 3 digits
		send_8bit_nibble(1, out_buffer[1]); // read from most significant bit
		send_8bit_nibble(1, out_buffer[2]);

		send_8bit_nibble(1, 0b01111111); // char "space"
		send_8bit_nibble(1, 0b00001110); // char "p"
		send_8bit_nibble(1, 0b01110010); // char "N"
		send_8bit_nibble(1, 0b01110010); // char "N"
		send_8bit_nibble(1, 0b01011100); // char: ":"

		temp_data = *NN50 * 100 / (*N_beats-2); // yy%
		int2str2(&temp_data, out_buffer); // convert SDNN data to string
		send_8bit_nibble(1, out_buffer[0]); // only interested in the first 2 digits
		send_8bit_nibble(1, out_buffer[1]);
        
		send_8bit_nibble(1, 0b10100100); // char "%"

	}

}



void debug_display(int *data1, int *data2, int *data3,
                   int *data4, int *data5, int *data6) {
    
	/*
	 * display six values on the LCD screen for debugging
     *
	 * */

	send_8bit_nibble(0, 0b10000000); // clear screen
	send_8bit_nibble(0, 0b01000000); // take display cursor home
	_delay_ms(15);

	uint8_t out_buffer[6]; // output container
    
    // data1
	int2str2(data1, out_buffer); // convert to string
	send_8bit_nibble(1, out_buffer[0]);
	send_8bit_nibble(1, out_buffer[1]);
	send_8bit_nibble(1, out_buffer[2]); 
	send_8bit_nibble(1, out_buffer[3]);
    
	send_8bit_nibble(1, 0b01011100); // char: ":"
    
    // data2 -- up to 5 digits
	int2str2(data2, out_buffer);
	send_8bit_nibble(1, out_buffer[0]); 
	send_8bit_nibble(1, out_buffer[1]);
	send_8bit_nibble(1, out_buffer[2]); 
	send_8bit_nibble(1, out_buffer[3]);
	send_8bit_nibble(1, out_buffer[4]);

	send_8bit_nibble(1, 0b01011100); // char: ":"
    
    // data3 -- up to 5 digits
	int2str2(data3, out_buffer);
	send_8bit_nibble(1, out_buffer[0]);
	send_8bit_nibble(1, out_buffer[1]);
	send_8bit_nibble(1, out_buffer[2]);
	send_8bit_nibble(1, out_buffer[3]);
	send_8bit_nibble(1, out_buffer[4]);
    
    // switch to second line
	send_8bit_nibble(0, 0b00000011);
    
    // data4
    int2str2(data1, out_buffer);
	send_8bit_nibble(1, out_buffer[0]); 
	send_8bit_nibble(1, out_buffer[1]);
	send_8bit_nibble(1, out_buffer[2]); 
	send_8bit_nibble(1, out_buffer[3]);

	send_8bit_nibble(1, 0b01011100); // char: ":"
    
    // data5
	int2str2(data2, out_buffer); // reuse buffer
	send_8bit_nibble(1, out_buffer[0]); 
	send_8bit_nibble(1, out_buffer[1]);
	send_8bit_nibble(1, out_buffer[2]); 
	send_8bit_nibble(1, out_buffer[3]);

	send_8bit_nibble(1, 0b01011100); // char: ":"
    
    // data6 -- up to 6 digits
	int2str2(data3, out_buffer);
	send_8bit_nibble(1, out_buffer[0]);
	send_8bit_nibble(1, out_buffer[1]);
	send_8bit_nibble(1, out_buffer[2]);
	send_8bit_nibble(1, out_buffer[3]);
	send_8bit_nibble(1, out_buffer[4]);
	send_8bit_nibble(1, out_buffer[5]);

}


