/*
 * Serial.cpp	-	Update	-	December 2025
 *
 *  Created on: Jun 13, 2023
 *      Author: Jon Freeman  B Eng (Hons) MIET
 *
 *      LAST MODIFIED 18th December 2025
 */

#include	<cstdio>
#include	<cstdbool>
#include	<cstring>

#include	"Serial.hpp"


struct	SerialPortPointers	{	//	Used to bind uartnum to port
	class Serial * port		{};
	UART_HandleTypeDef* uart	{};
};

SerialPortPointers	MySerialPorts	[MAX_NUMOF_UARTS];	//	Constructor(s) builds table of ports

#if 0
void	myportsreport	()	{
	int	len;
	char	t[100];
	for	(int i = 0; i < MAX_NUMOF_UARTS; i++)	{
		len = sprintf	(t, "Port %d, this-> %ld, huartn %ld\r\n", i, (uint32_t)MySerialPorts[i].port,
				(uint32_t)MySerialPorts[i].uart);
//		pc.write(t, len);
		MySerialPorts[0].port->write(t, len);
	}
}
#endif

void	Serial::Constructor_Core	()	{
	int i = 0;
	while	(i < MAX_NUMOF_UARTS)	{
		if	(!(MySerialPorts[i].uart))	{	//got empty slot
			MySerialPorts[i].uart = m_huartn;
			MySerialPorts[i].port = this;
			i = MAX_NUMOF_UARTS;
		}
		i++;
	}
    if (ring_outbuff == nullptr) {
        serial_error |= 1;				//	Flag fatal buffer allocation failure
    }
}


Serial::Serial	(UART_HandleTypeDef &which_port)	//	Constructor
	:	m_huartn {&which_port}
{	//	Constructor
	lin_inbuff 		= new (std::nothrow) char[DEFAULT_LIN_INBUFF_SIZE + 4] { 0 }	;
	ring_outbuff 	= new (std::nothrow) char[DEFAULT_RING_OUTBUFF_SIZE + 4] { 0 }	;
	Constructor_Core	()	;
}	//	End of constructor

Serial::Serial	(UART_HandleTypeDef &which_port, const size_t tx_buffsize)
	: m_huartn {&which_port}
	, tx_ringbuff_size {tx_buffsize} 	//	Constructor
{
	lin_inbuff 		= new (std::nothrow) char[DEFAULT_LIN_INBUFF_SIZE + 4] { 0 }	;
	ring_outbuff 	= new (std::nothrow) char[tx_ringbuff_size + 4] { 0 }	;
	Constructor_Core	()	;
}	//	End of constructor


bool	Serial::start_rx	()	{	//	Call from startup function. Call from constructor fails, Too Early !
	if	(HAL_OK == HAL_UART_Receive_DMA	(m_huartn, rxbuff, 1))	//	huartn and rxbuff 'private'
		return	(true);
	set_error	(SerialErrors::HAL_UART);
	return	(false);
}


void	Serial::set_tx_busy	(bool true_or_false)	{
	tx_busy = true_or_false;
}


bool	Serial::get_tx_busy	()	{	//	Returns true when DMA in progress
	return	(tx_busy);
}

#define	BACKSPACE	127

//extern	void	frequd	(int)	;
//extern	void	voltud	(int)	;

void	special_char_handler	(char	c)	{
/*	switch	(c)	{
	case	65:		//	Up
		frequd	(1);
		break;
	case	66:		//	Down
		frequd	(-1);
		break;
	case	67:		//	Right
		voltud	(1);
		break;
	case	68:		//	Left
		voltud	(-1);
		break;
	default:
		break;
	}
	*/
}

bool	Serial::get	(uint8_t & a)	{	//	If char avail, is copied into 'a' and returns true. Returns false if no char available
	return	(newRx.get(a));
}

/**	bool	Serial::read	(uint8_t * buff, size_t & max_len)
 *
 * New 2025 Jun 30th
 *
 * Returns false if no characters available to be read, max_len set to 0
 * Returns true on success having read up to 'max_len' characters into buff.
 * max_len returns number of characters read
 */
bool	Serial::read	(uint8_t * buff, size_t & max_len)	{
	size_t	posn = 0;
	if	(newRx.empty())	{
		max_len = 0;
		return	(false)	;
	}
	buff[0] = 0;
	while	((posn < max_len) && (newRx.get(buff[posn])))	{
		buff[++posn] = 0;
	}
	max_len = posn;
	return	(true)	;
}



//uint8_t	*	get_message	()


/**
 * char *	Serial::test_for_message	()	{	//	Updated Aug 2025
 *
 * Called from ForeverLoop at repetition rate
 * Returns pointer to lin_inbuff when "\r\n" terminated command line has been assembled in lin_inbuff, NULL otherwise
 */
char *	Serial::test_for_rx_message	()	{	//	Read in any received chars into linear command line buffer
	while	(newRx.get(ch[0]))	{			//	While there are received chars to be processed
		if	(lin_inbuff_onptr >= DEFAULT_LIN_INBUFF_SIZE)	{	//
			ch[0] = '\r';		//	Prevent command line buffer overrun
			set_error (SerialErrors::INPUT_OVERRUN);	//	set error flag
		}
		switch	(in_esc_seq)	{
		case	0:	//	Not in ESC sequence, nothing to do
			break;
		case	1:	//	prev char was ESC, next need to see 91 if valid esc seq
			if	(ch[0] == '[')	{
				in_esc_seq = 2;	//	Proceed to harvest next char as special
				ch[0] = '\n';
			}
			else
				in_esc_seq = 0;	//	Abort ESC sequence
			break;
		case	2:	//	Test for 65 (up), 66 (dn), 67 (rt), 68 (lt)
//			if	((ch[0] < 65) || (ch[0] > 68))
			special_char_handler	(ch[0]);
			ch[0] = '\n';
			in_esc_seq = 0;
			break;
		default:
			break;
		}
		if	(ch[0] == 27)	{
			in_esc_seq = 1;
			ch[0] = '\n';	//	force act no further on ESC
		}
		if(ch[0] != '\n')	{	//	Ignore newlines
#if 0
			char	t[32];
			int	len;
			len = sprintf	(t, "%d ", ch[0]);
			pc.write	(t, len);
#endif
			lin_inbuff[lin_inbuff_onptr++] = ch[0];
			lin_inbuff[lin_inbuff_onptr] = '\0';
			if(ch[0] == '\r')	{
				lin_inbuff[lin_inbuff_onptr++] = '\n';
				lin_inbuff[lin_inbuff_onptr] = '\0';
//??	NO NO NO			write	(lin_inbuff, lin_inbuff_onptr);	//	echo received command string to originator
				lin_inbuff_onptr = 0;	//	Could return the length here, might be useful
				if	(test_error(SerialErrors::INPUT_OVERRUN))	{
					write	("INPUT_OVERRUN_ERROR\r\n", 21);
					clear_error	(SerialErrors::INPUT_OVERRUN);
				}	//	End of if	(test_error(SerialErrors::INPUT_OVERRUN))	{
				time_ms_of_most_recent_rx = HAL_GetTick();
				return	(lin_inbuff);			//	Got '\r' command terminator

			}	//	End of if(ch[0] == '\r')	{
			else if	(ch[0] == BACKSPACE)	{
				lin_inbuff[--lin_inbuff_onptr] = '\0';
				if	(lin_inbuff_onptr > 0)
					lin_inbuff[--lin_inbuff_onptr] = '\0';
			}	//	End of else if	(ch[0] == BACKSPACE)	{
		}
	}
	return	(nullptr);	//	Have not yet found any complete terminated command string to process
}


void	Serial::report_error	()	{
	if	(serial_error != 0)	{
		serial_error_history	|= serial_error;
		char	t[44];
		int		len;
		len = sprintf	(t, "SERIAL ERROR CODE %ld\r\n", serial_error);
		write	(t, len);
		clear_error(SerialErrors::ALL);
//		serial_error = 0;
	}
}
//bool	Serial::test_error	(int mask)	const	{	//	Return true for error, false for no error
bool	Serial::test_error	(SerialErrors mask)	const	{	//	Return true for error, false for no error
	return	(serial_error & static_cast<uint32_t>(mask));	//	true for NZ result (error flag set), false for 0 or no error result
}


uint32_t	Serial::clear_error	(SerialErrors bit)	{	//
//	serial_error &= ~(1 << static_cast<int>(bit));
	serial_error &= ~(static_cast<uint32_t>(bit));	//	From Dec 2024
//	serial_error &= ~(std::to_underlying(bit));	//	C++23
	return	(serial_error);
}


void	Serial::set_error	(SerialErrors bit)	{
//	serial_error |=	(1 << static_cast<int>(bit));
	serial_error |=	(static_cast<uint32_t>(bit));		//	From Dec 2024
}


/**
 * void	Serial::write	(const uint8_t * source, int len)	{
 *
 *		A Non-Blocking function
 *		Data to be written is identified by pointer to source, and length.
 *		We must assume data is in a local buffer within the calling function,
 *		and that this memory will be trashed when the calling function completes.
 *
 *		This makes it essential for this write function to copy the source data
 *		into a non-volatile memory area
 *
 * */
bool	Serial::write	(const uint8_t * source, int32_t len)	{	//	Only puts chars on buffer.
	if	(len < 1)
		return	(false);			//	Can not send fewer than 1 chars !
	int32_t	buff_space = ring_outbuff_offptr - ring_outbuff_onptr;
	if	(buff_space <= 0)
//		buff_space += DEFAULT_RING_OUTBUFF_SIZE;
		buff_space += tx_ringbuff_size;
	if	(buff_space < len)	{
		set_error (SerialErrors::OUTPUT_OVERRUN);	//	Have proved this works
		return	(false);							//	Not room to send whole message so send none of it
	}		//	To pass here, have space to put len bytes on buffer


/*
 * No no no, July 2025
 * This was trying to be too clever and wasn't correct
 *
 * 	int	space_to_bufftop = RING_OUTBUFF_SIZE - ring_outbuff_onptr;
	char *	dest1 = ring_outbuff + ring_outbuff_onptr;
	if	(len > space_to_bufftop)	{
//		set_error (SerialErrors::SOME_OTHER);	//	Proved no problem here
		memmove	(dest1, source, space_to_bufftop);
		memmove	(ring_outbuff, source + space_to_bufftop, len - space_to_bufftop);
		ring_outbuff_onptr += len;	//	which takes us beyond end of buffer
		ring_outbuff_onptr -= RING_OUTBUFF_SIZE;
	}
	else	{
		memmove	(dest1, source, len);
		ring_outbuff_onptr += len;
	}*/

	size_t	cntjuly25 = 0;		//	This works !
	while	(len--)	{
		*(ring_outbuff + ring_outbuff_onptr) = source[cntjuly25++];
//		++ring_outbuff_onptr %= DEFAULT_RING_OUTBUFF_SIZE;
		++ring_outbuff_onptr %= tx_ringbuff_size;
	}

	tx_buff_empty = false;
	tx_any_buffered	()	;	//	Perhaps better to leave this to be done regularly from forever loop - No Aug 2025, should be good, can't see why not
	return	(true);
}


bool	Serial::write	(const char * source, int32_t len)	{	//	Remembering to keep type-casting is such a bore
	return	(write	((uint8_t*)source, len));					//	Overloaded functions take char or uint8_t
}


bool	Serial::tx_any_buffered	()	{	//	Call this often from forever loop
	if	(tx_buff_empty || tx_busy)		//	tx_busy true while DMA active
		return	(false);
	//	To be here, tx_buff has stuff to send, and uart tx chan is not busy

	size_t	len = 0;
	while	(!tx_buff_empty && (len < LIVE_TXBUFF_SIZE))	{
		//
		live_tx_buff[ len++ ] = ring_outbuff[ring_outbuff_offptr++];
//		tx_buff_full = false;
//		ring_outbuff_offptr %= DEFAULT_RING_OUTBUFF_SIZE;		//	circular buffer
		ring_outbuff_offptr %= tx_ringbuff_size;		//	circular buffer
		if(ring_outbuff_onptr == ring_outbuff_offptr)
			tx_buff_empty = true;
	}
	if	(len > 0)	{
		tx_busy = true;
		if	(HAL_OK == HAL_UART_Transmit_DMA	(m_huartn, (uint8_t *)live_tx_buff, len))
			return	(true);
		set_error	(SerialErrors::HAL_TX_DMA);	//	HAL was not OK (Dec2024)
	}
return	(false);
}



//	USART Interrupt Handlers

void	Serial::rx_intrpt_handler_core_DMA	()	{	//	UART has rec'd a single char, DMA has already placed it on circular buffer.

	newRx.put(rxbuff[0]);	//	New 2025 CircularBuffer. Uart Rx DMA has placed fresh char at rxbuff[0]. This copies it into circular buff
	start_rx	()	;		//	Set DMA ready to receive next char into rxbuff[0]
}	//	End of Serial::rx_intrpt_handler_core_DMA	()


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)	{
	int i = 0;
	while	(MySerialPorts[i].port)	{
		if	(huart == MySerialPorts[i].uart)	{
			MySerialPorts[i].port->rx_intrpt_handler_core_DMA();
			return	;
//			MySerialPorts[i].port->newRx.put(rxbuff[0]);	//	New 2025 CircularBuffer
//			MySerialPorts[i].port->start_rx	()	;
		}
		i++;
	}
}

//void HAL_UART_DMATxCpltCallback(UART_HandleTypeDef *huart)	//	This called as well as HalfCplt
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)	//	This called as well as HalfCplt
{
	int i = 0;
	while	(MySerialPorts[i].port)	{
		if	(huart == MySerialPorts[i].uart)
			MySerialPorts[i].port->set_tx_busy(false);
		i++;
	}
}	//	End of void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)


void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	int i = 0;
	while	(MySerialPorts[i].port)	{
		if	(huart == MySerialPorts[i].uart)
			MySerialPorts[i].port->set_error	(SerialErrors::HAL_UART);
		i++;
	}
}


