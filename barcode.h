#ifndef _BARCODE_H_
#define _BARCODE_H_

#include <Mega32.h>


/* DEBUG MODE? uncomment for debug with terminal*/
#define DEBUG     

/* USART product database interface uncomment to activate terminal*/
#define USART 

#include <stdio.h>  
#include <lcd.h>

#ifdef USART
	#include <stdlib.h>    
#endif    

#define TRUE 1
#define FALSE 0 

#define LCDwidth 16

/* barcode_read.c */
//function prototypes
void Init(void);
void UpdateCodeState(void);
unsigned char FormCodeDigit(unsigned char digitnum, unsigned char parity);
unsigned char IsDigitInSet(unsigned char digit, unsigned char set[10]) ; 
unsigned char GetDigitParity(unsigned char digit);


/* barcode_decode.c */
//function prototypes 
int decode(unsigned char* scanned);
int getFirstDigit(void);
int CRC(void);
void PrintBarcode(void);
int getCountry(void);
#ifdef USART  
void getDataFromUSART(void);
#endif

// Global variables
#define ERROR -1

//digit parity
#define ODD 1
#define EVEN 0
#define RIGHT 2
#define NONE 3    
     
#endif                                                                                                                                                 