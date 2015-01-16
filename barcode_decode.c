#include "barcode.h"  

extern char lcd_buffer[17]; 

//Macros
#define SCANNEDSIZE 12
#define NUMDIGITS 10
#define BARCODE 13

// Flash arrays  
#define COUNTSIZE 8  
flash char* countries[8] = {"USA & Canada", "France",
							"Germany", "Japan", "Mexico",
							"ISBN", "ISMN", "ISSN"};

#define MAPSIZE 8
flash int mapping[8] = {0, 37, 44, 45, 750, 978, 979, 980};
 
flash int pat[10] = {31, 20, 18, 17, 12, 6, 3, 10, 9, 5};  //patterns
int pattern; //track parity pattern 

extern unsigned char odd[10];
extern unsigned char even[10];
extern unsigned char right[10]; 

unsigned char barcode[13];

#ifdef USART
char c_in;
char input_buffer[13];
unsigned char input_count; 

#define INPUT_PRICE 0
#define INPUT_NAME  1 
#define INPUT_DONE  2
unsigned char input_state;   

float fprice;
#define MAX_INPUT 9      



interrupt[USART_RXC] void usart_rx(void)
{
	c_in = UDR;
	UDR = c_in;
	
	if(input_state == INPUT_PRICE) {
     	if( c_in != '\r' )
     		input_buffer[input_count++] = c_in;
     	else
     	{         		
     		UCSRB.7 = 0; //int off       		
     		putchar('\n');
     		input_buffer[input_count] = NULL;
     		fprice = atof(input_buffer);
     		input_count = 0;
     		printf("Enter Name(MAX 8 char):");
     		input_state = INPUT_NAME;
     		UCSRB.7 = 1;
     	}  
 	}
 	else if(input_state == INPUT_NAME){
 	     if( c_in != '\r' )
     		input_buffer[input_count++] = c_in;
     	else
     	{
     		UCSRB.7 = 0; //int off
     		putchar('\n');
     		input_buffer[input_count] = NULL;		
     		input_state = INPUT_DONE; 
     	}     	   
 	}     	
}

void getDataFromUSART(void) {
    
    
    printf("Enter price: ");
    input_count = 0; 
    input_state = INPUT_PRICE;
    UCSRB.7 = 1;   //interrupt ON   
    
    while(input_state != INPUT_DONE)   //wait on input...
    	;                         
    	
    sprintf(lcd_buffer,"$%.2f",fprice); //print price   
    lcd_gotoxy(0,0);
    lcd_puts(lcd_buffer);    
    sprintf(lcd_buffer," %s",input_buffer); //print name
    lcd_puts(lcd_buffer);    
}      
#endif

void PrintBarcode(void)
{
	int i, country;

	if(barcode == NULL) return;
	
	country = getCountry();
	
	lcd_clear();
	lcd_gotoxy(0,1);
	
	#ifdef USART
	printf("\r\nUPC = ");
	#endif
	
	for(i = 0; i < BARCODE; i++)
	{
		// Insert Dashes at certain places
		if( (country == 0 && i == 1) || (1 <= country && country <= 3 && i == 2) || (country>3 && i==3) || i==12 ) 
			lcd_putsf("-");
			
		sprintf(lcd_buffer, "%c", barcode[i] +'0');
		lcd_puts(lcd_buffer);
		
		#ifdef USART     
		printf("%s",lcd_buffer);		
		#endif	
	}         
	
	#ifndef USART   	
	lcd_gotoxy(0,0); 
	if(ERROR == country)
		lcd_putsf("Unknown Country");
	else
		lcd_putsf(countries[country]); 
	#endif   
	
	#ifdef USART    
	printf("  (");
	if(ERROR == country)
		printf("Unknown Country");
	else
		printf(countries[country]);
	printf(")\r\n");
	#endif   

}


/*
 * check the decoded barcode versus its control.
 */
int CRC(void)
{
	int accumulator = 0;
	int i;
	
	// iterate through barcode.
	for(i = 0; i < SCANNEDSIZE; i++)
	{
		if(0 == i%2)
			accumulator += barcode[i];
		else
			accumulator += 3 * barcode[i];
	}
	if(barcode[12] == (10 - accumulator%10) )
		return TRUE;   
	else 	 
		return FALSE;	 
}

/*
 * Determine the first digit from the Odd/Even 
 * pattern of the manufacture's code
 */
int getFirstDigit(void)
{  
	int k = 0;
	
	// iterate through digits.
	while(k < NUMDIGITS)
	{
		if( pat[k] == pattern )
		{
			return k;
		}
		k++;
	}
   
	//This ouputs correctly.
	lcd_clear();
	lcd_gotoxy(0,0);
	lcd_putsf("Error:");
	lcd_gotoxy(0,1);
	lcd_putsf("Bad Pattern");
	return ERROR;
}

/*
 * Given an array of characters that represents a barcode
 * decode the array into its individual characters and 
 * return the decoded barcode
 */
int decode(unsigned char* scanned)
{	
	int i, j;
	char temp;
	int format;
	pattern = 0;
	
	for(i = 0; i < SCANNEDSIZE; i++)
	{
		temp = scanned[i];
		j = 0;
		format = NONE;
		
		// iterate through digits.
		while(j < NUMDIGITS)
		{
			// path for first 6 characters
			if( i < 6)
			{
				// Check values in the even and odd arrays versus temp.
				if(odd[j] == temp)
				{
					format = ODD;
					break;
				}
				else if(even[j] == temp)
				{
					format = EVEN;
					break;
				}
			}
			else // path for last six characters
			{
			 	if(right[j] == temp)
				{	
					format = RIGHT;
					break;
				}
			}
			
			j++;
		}
		
		// Check that while loop ended via a break;
		// If not, then unknown charcter format.
		if(format == NONE)
		{
			// This outputs correctly.  
  			lcd_clear();
  			lcd_gotoxy(0,0);
  			lcd_putsf("Error:");
  			lcd_gotoxy(0,1);
	 		lcd_putsf("Bad Format.");
	 		return ERROR;
		}
		else
		{
	   		// Store pattern and decoded digit.
	   		if( 0 < i && i < 6)
	   			pattern = (pattern << 1) + format;
	   		
	   		barcode[i+1] = j;
		}		
	}
	
	if(ERROR == (barcode[0] = getFirstDigit() ) )
		return ERROR;
	
	if( FALSE == CRC() )
	{
	   // This ouputs correctly.
		lcd_clear();
		lcd_gotoxy(0,0) ;
 		lcd_putsf("Error:");
 		lcd_gotoxy(0,1);
		lcd_putsf("Failed CRC.");    
		return ERROR;
	}
	return TRUE;
}

/*
 * Identifies Barcode country origin and returns index into country array.
*/
int getCountry(void)
{
	long country;
	int i;
	
	if(barcode[0] == mapping[0])
		return 0;
	else
	{
		country = 10*(10*(long)barcode[0] + (long)barcode[1]) + (long)barcode[2];
		for(i = 1; i < MAPSIZE; i++)
		{
			if(country == mapping[i])
				return i; 
		}
		return ERROR;
	}
}