//0x15 is I/O Address of PortC.   
#asm
 .equ __lcd_port = 0x15 
#endasm   
#include <barcode.h>    

#define BLACK 1      //scan colors
#define WHITE 0
unsigned char read;      //reading from scanner: black 1, white 0  
unsigned char last_read; //previous bit scanned

#define TIMEOUT 80 //how soon to timeout a read
#define MAX_DIGITS	12
int seg_len[MAX_DIGITS+1][4]; 	//for each digit, length of bar or space segments in current digit     
int cur_seg_len=0; 
			        		//as read in initially, before scaling                      
unsigned char trans = 0; //transitions in character read so far    
unsigned char digit_count = 0; //read thru what digit in the barcode  
unsigned char scan_done = 0;  //initial scan complete?    

char lcd_buffer[17]; 
       
//scanning barcode state machine
#define CODE_WAIT	0  //waiting to start scanning a barcode, sitting on white surface
#define CODE_LGUARD 1  //reading in the left guard
#define CODE_LDIGIT	2  //reading the 6 'left' digits
#define CODE_CGUARD	3  //reading in the center guard
#define CODE_RDIGIT 4  //reading the 6 'right' digits
#define CODE_RGUARD 5  //reading right guard, then will move to wait
unsigned char code_state = CODE_WAIT; //state in reading the code
unsigned char code_bits[MAX_DIGITS];  //final barcode bit array  

//parity tables, set of bit codes
unsigned char odd[10] = {13, 25, 19, 61, 35, 49, 47, 59, 55, 11};
unsigned char even[10] = {39, 51, 27, 33, 29, 57, 5, 17, 9, 23};
unsigned char right[10] = {114, 102, 108, 66, 92, 78, 80, 68, 72, 116};
     
/* the following two time critical functions do NOT save all registers on call,
   the status register is saved manually.
*/

#pragma savereg-   
/* Timer1 compare match A interrupt, reads from the barcode, updates state
   based on transitions. They intentionally operate only on global variables 
   Fills a 2D array for each digit, saving the width of each bar/space
*/
interrupt[TIM1_COMPA] void read_timer(void)
{         
	#asm
		st -Y, R16
		in R16, SREG
		st -Y, R16
	#endasm 
	
	last_read = read;
 	read = PINB.0;  //read from the scanner TTL
         
 	if(read != last_read) {  //transition found    
 		seg_len[digit_count][trans] = cur_seg_len;  //save the length  
 		++trans;	
		cur_seg_len = 0;//reset
   	}
	
	++cur_seg_len; //increment length of current segment 	        
	UpdateCodeState(); //update the code scan state 
	if(cur_seg_len > TIMEOUT) {   //time out?        
		trans = 0;     
		cur_seg_len = 0;   
		code_state = CODE_WAIT; 
	}       	  
     #asm              
     	ld R16, Y+
		out SREG, R16 
		ld R16, Y+
	#endasm 
}    
#pragma savereg+

#pragma savereg-  
/* Update the Code Scan state 
   State transitions based on count of transitions, each zone guaranteed
   to have a certain number.
*/
void UpdateCodeState(void) {   
	#asm
		st -Y, R16
		in R16, SREG
		st -Y, R16
	#endasm 
                      

	switch(code_state)
	{    
		case CODE_WAIT:  //sitting on white, expecting to read   
			if((last_read == WHITE) && (read == BLACK)) { //start of left guard?
				code_state = CODE_LGUARD;         
				trans = 0;
			}
			break;    
			
		case CODE_LGUARD:  //scanning left guard bar
			if(trans == 3) {//back to white, guard ended   
				code_state = CODE_LDIGIT;
				trans = 0;   
				digit_count = 0;       
			}
	    		break;       
	    		
		case CODE_LDIGIT: //scanning the six left digits 		
			if(trans == 4) { //digit complete  
			     ++digit_count; 
			     trans = 0;  
			     if(digit_count == 6) {//reached center guard
			     	code_state = CODE_CGUARD;          
			     }  
			}
			break;     
			
		case CODE_CGUARD:  //scanning center bar
		     if(trans == 5) {	
		     	code_state = CODE_RDIGIT;     		     	
		     	trans = 0;
		     }
			break;    
			
		case CODE_RDIGIT: 	 
			if(trans == 4) { //digit complete      
			     ++digit_count;  
			     trans = 0;           
			     if(digit_count == 12) \//reached center guard   
			     	code_state = CODE_RGUARD;   
			}
			break;   
			
		case CODE_RGUARD:    
			if(trans == 3) { //guard finished
				code_state = CODE_WAIT;	 
				scan_done = 1;  
			}
			break; 
	}  //end switch  
	
	#asm              
     	ld R16, Y+
		out SREG, R16 
		ld R16, Y+
	#endasm 
}//end UpdateState  
#pragma savereg+                

/*FormCodeDigit
    	-For a particular digit, requiring a certain parity
	-Takes the segment (bar/space) lengths and scales them to a total width of 7
	-Uses remainders to adjust for integer math, greatest will be incremented, since it
	is the closest to being the next width up
	-If parity does not fit, will find a single bit adjustment to fit it.  The most 
	common misread is a width of 4 being seen as only 3. If a 3 is present, it is first
	switched to a 4, and the single 2 to a 1 and its parity is checked.  If this fails, then
	the next match is accepted.        
*/
unsigned char FormCodeDigit(unsigned char digitnum, unsigned char parity) { 

	 int len=0, i, j, tot_width,temp;
      int seg_width[4], seg_width_mod[4]; 
      unsigned char digit, cur_color, reform, cur_parity, try3to4;  
      unsigned char take=0, give=1; //take is segment that will increase by one in width, give decrease
      int temp_seg_width[4];        //during error correction
               
      digit = 0;
	 len = 0;
 	 tot_width = 0;    
 	 
      for(i = 0; i < 4; i++)
      	len = len + seg_len[digitnum][i]; //total length of reading   
         
      //get the width of spaces and bars : max of 7
      for(i = 0; i < 4; i++) {
      	temp = seg_len[digitnum][i] * 7;
      	seg_width[i] = temp/len;                               
      	if(seg_width[i] == 0) { //account for round down to 0, bring to 1            
      		seg_width[i] = 1; 
      		seg_width_mod[i] = 0;  
      	}  
      	else if(seg_width[i] > 4) {//can't have wider than 4
      	 	seg_width[i] = 4;
      	 	seg_width_mod[i] = 0;  
      	}
      	else                   //never get a 5, eff. account for mod's percentage of width
      		seg_width_mod[i] = (seg_len[digitnum][i]%len) * (4 - seg_width[i]);   
      	#ifdef DEBUG
      		printf("%d,",seg_len[digitnum][i]);
      	#endif     	
      	tot_width += seg_width[i];
      }     
      #ifdef DEBUG
      	printf(" => ");
      #endif
  
      if(tot_width > 7)
      	return 0;       
      	
      //round down may result in fewer than 7 elements
      //so round up the next closest until 7 reached, based on remainders	
      while(tot_width < 7)  {                 
      	temp = 0;    
        	for(i = 1; i < 4; i++)  {
        	 	if(seg_width_mod[i] > seg_width_mod[temp])        			
         	 		temp = i;                              
         	 	else if( (seg_width_mod[i] == seg_width_mod[temp]) && (seg_width[i] < seg_width[temp]) ) 
         	 		temp = i;  //smallest breaks tie
        	}	
         	++seg_width[temp]; //increase the winner of comparison    
         	seg_width_mod[temp] = 0; //clear from candidates
         	++tot_width; 
      }                  
      
      for(i = 0; i < 4; i++) 
      	temp_seg_width[i] = seg_width[i];  //copy to temp to initialize
      
      reform = 1; 
      try3to4 = 0;      //if there is a 3, first try getting it to a 4                
      for(i = 0; i < 4; i++) {   
      	if(temp_seg_width[i] == 3) {
          	try3to4 = 1;           
          	take = i;  //initialize take to the 3, it will then wrap around if that fails
       	}    
       	else if(temp_seg_width[i] == 2)
       		give = i;
      }
      if(!try3to4) //if no 3 to 4 attempt to be made, reset give
      	give = 1;
      
      while(reform) {  
           //generate the digit from the 4 widths and start color                 
           cur_color = (parity == RIGHT) ? BLACK : WHITE;     //LEFT is WHITE, RIGHT is BLACK
           for(i = 0; i < 4; i++) {
          	 for(j = 0; j < temp_seg_width[i]; j++) {
            		digit = digit << 1;
            		digit |= cur_color;
           	 }
           	 cur_color = !cur_color;  
           	 #ifdef DEBUG
           	 	printf("%d,",temp_seg_width[i]);
           	 #endif
           }      
           digit = digit & 0x7f; //crop off first bit
           
           #ifdef DEBUG  
           	printf("\r\n");
           #endif 
          
          //Is this digit of valid, correct parity? if not, perform error correction reforming   
          cur_parity = GetDigitParity(digit);                                      
          if( (parity == RIGHT) && (cur_parity == RIGHT) ) 
          	reform = 0;
          else if( (parity == ODD) && (cur_parity == ODD ) ) //|| (cur_parity == EVEN) ) //in left, accept ODD or EVEN
               reform = 0;     
          else if( (parity == NONE) && ((cur_parity == ODD) || (cur_parity == EVEN)) )
          	reform = 0;                          
                           
          if(reform && take < 4) {
      		for(i = 0; i < 4; i++) //operate on temp only
      			temp_seg_width[i] = seg_width[i];  
      		
      	     #ifdef DEBUG
      	     	printf("reforming... ");  
      	     #endif 		
      		while(1) {
      			if(try3to4) {//give and take already set up, try the adjustment  
      		      	temp_seg_width[give] = 1;
      		      	temp_seg_width[take] = 4;
      		      	take = 0;
      		      	give = 1;
      		      	try3to4 = 0;
      		      	break;	        
      		 	}
      			else if(take == 4) { //given up, didn't start in middle 
      				reform = 0;   
      			}
      			else if(give == 4) { //done for this taker
               		give = 0;     
               		++take;             
               	}
      			else if(take == give)
               		++give; //skip self  
                	else if(temp_seg_width[take] == 4) { // cannot take
                      	give = 0;
                	     ++take;
               	}    
               	else if(temp_seg_width[give] == 1) //unable to give
               		++give;
               	else { //able to take and give
               		--temp_seg_width[give];
               	     ++temp_seg_width[take]; 
               	     ++give;                     	      
               	     break;
               	}  
          	}//end while
          }//end if reform && take < 4           
      } //end while reform     
      
      return digit; 
}          

/* IsDigitInSet
   Checks if particular digit is in the given set
*/   
unsigned char IsDigitInSet(unsigned char digit, unsigned char set[10]) {
      
	unsigned char i;
	
	for(i = 0; i < 10; i++) 
	 	if(digit == set[i])
	 		return 1;	
		
	return 0;      
}      

/*GetDigitParity
  Returns digits parity
*/  
unsigned char GetDigitParity(unsigned char digit) {

 	if( IsDigitInSet(digit, odd) )
 		return ODD;
 	if( IsDigitInSet(digit, even) )
 		return EVEN;
 	if( IsDigitInSet(digit, right) )
 		return RIGHT;
 		
 	return NONE;
}
     
//Initialize the barcode scanner
void Init(void)
{    
	DDRB.0 = 0;  //input for reader TTL 
	PORTB.0 = 1;  //use internal pull-up 
	
	DDRC = 0xff; //LED test
	PORTC = 0xff; //OFF

	//configure timer1 for fastest compare matches
	TCCR1B = 0x01;	//no prescalar.
	OCR1A = 1;   

	TIMSK = 0x10; //enable timer1 compare match int.

	//Initialize USART

	#ifdef DEBUG  
		#ifndef USART
	     UCSRB = 0x08; //receive and transmit, receive int off
     	UBRRL = 103; //9600 baud    
     	printf("\r\n-DEBUG MODE-!\r\n\n");
     	#endif  ; 
     #endif 
     #ifdef USART
 		UCSRB = 0x18; //receive and transmit, receive int off
     	UBRRL = 103; //9600 baud  
     	printf("\r\n\n--------------------\r\n");    
  		printf("Fake Barcode Database!\n\r"); 
  		printf("---------------------\r\n"); 
  		#ifdef DEBUG
     	printf("\r\n-DEBUG MODE-!\r\n\n");
  		#endif  
  		
	#endif  
	
	lcd_init(LCDwidth);    //Initialize LCD
	lcd_clear();             
	lcd_gotoxy(0,0);
	sprintf(lcd_buffer, "Ready");
	lcd_puts(lcd_buffer);
	
   	//Globally enable interupts.  
   	#asm
   		sei
   	#endasm	   
}	
              
void main(void)
{    
	unsigned char i;
	Init();

	while(1){                
      	if(scan_done) {	//code read complete 
      		TIMSK = 0;  //disable interrupt for now...
      		 
      		//form the barcode bit array
      		code_bits[0] = FormCodeDigit(0, ODD); //first has to be odd      
          	for(i = 1; i < 6; i++)  //Finish Left digits
          		code_bits[i] = FormCodeDigit(i, ODD); //could be odd or even (NONE)
          		                                      //if ODD - UPC-A only     
          	#ifdef DEBUG	                                      
          		printf("\r\n");
          	#endif     
          	
          	for(i = 6; i < MAX_DIGITS; i++) //Right digits
          		code_bits[i] = FormCodeDigit(i, RIGHT); 
          		 
          	#ifdef DEBUG	                                      
          		printf("\r\n");
          	#endif          	
          	
          	//DECODE the bits
          	if( ERROR != decode(code_bits) ) {
          		PrintBarcode(); 
          		#ifdef USART 
          	    		getDataFromUSART(); 
          	 	#endif
          	}
          	  		
       		scan_done = 0;        		                	
       		TIMSK = 0x10;      //turn interrupt back on      
      	}      
 	}                           
	
}