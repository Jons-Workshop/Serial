#pragma once
/*
 * Serial.hpp  Created on: Jun 13, 2023	-	Update	-	19th August 2026
 *
 *      Author: Jon Freeman  B Eng (Hons) MIET
 *      NOTE This does NOT initialise hardware.
 *      To use any uart you must first initialise hardware, baud, parity etc by setting up UART and DMA in .ioc file
 *
 *	Use	-		Serial	my_port	(UART_HandleTypeDef &which_port)	;	//	Use default tx buffer size
 *	Use	-		Serial	my_port	(UART_HandleTypeDef &which_port, tx_buffsize)	;
 *	Use	-		Serial	my_port	(UART_HandleTypeDef &which_port, tx_buffsize, rx_buffsize)	;
 *	e.g.		Serial	pc	{huart2, 3000, 250};
 *
 *      LAST MODIFIED 19th August 2026
 *
	//	Do NOT call 'start_rx' functions until after 'main ()' has started
 *
 *      Oct 2024
 *      DMA Usage
 *
 *      DMA is perfect for Tx as we can give any length to send, doing it by interrupt would mean one interrupt per char
 *      DMA for Rx? Maybe, but typ looking for random length input with chars arriving at random times
 *       ** UPDATE ** Reverted to _IT on Rx due to observed problem, which turned out to be dodgy RS232 USB lead
 *      seemingly erratic problems with DMA closing reception
 */
#ifndef INC_SERIAL_HPP_
#define INC_SERIAL_HPP_

#include	"main.h"	//	is needed here

#include	<new>		//	needed to handle std::nothrow

#define	USING_RX_DMA	//	Choice between Rx DMA or IT (interrupt). Probably not much to choose - Check DMA enabled in MX

template <class T>	class	CircularBuffer	{	//	Class to manage circular buffer of type T objects

	const	size_t	buffsize;					//	Initialised by parameter 's' brought to Constructor
	T * buffer;	//	buffer space allocated and properly created by constructor
	bool		full	{ false };
	size_t		onptr	{ 0L };
	size_t		offptr	{ 0L };
	uint32_t	cberrs	{ 0L };	//	low order bits set by various errors - in progress

public:
	T	txch ;	//	temp scratch variable

	CircularBuffer	(const size_t size)		//	buffer size is required parameter
		: buffsize { size }	{
		buffer = new (std::nothrow) T[buffsize + 2] { 0 }	;	//	allocate memory for buffer of required size
        if (buffer == nullptr) {
        	cberrs = 1;				//	Flag fatal buffer allocation failure
        }
	}	//	Parameter is buffer size

	~CircularBuffer	()	{	delete buffer;	}   // Destructor

	uint32_t	get_cb_errors	() const	{	return	(cberrs);	}


	void	clr_errors	()	{	cberrs = 0;	}


	bool	empty	() const	{	return	(!full && (onptr == offptr));	}


	bool	get		(T & element)	{	//	Returns false on buffer empty fail. Type T element could be of any size
		if	(empty())
			return	(false);
		element = buffer[offptr++];		//	Get element from buffer. Post increment pointer
		offptr %= buffsize;
		full = false;					//	Have removed one element. Therefore can not be 'full'
		return	(true);					//	Success
	}


	T *	get	()	{			//	Non preferred method for most. Returning address means type T object could be of any size
		T *	rv;				//	return value
		if	(empty())
			return	(nullptr);
		rv = (buffer + offptr++);		//	Get element address. Post increment pointer
		offptr %= buffsize;
		full = false;					//	Have removed one element. Therefore can not be 'full'
		return	(rv);					//	Success
	}


	bool	put		(T & element)	{	//	Returns false on buffer full fail
		if	(full)	{					//	Does not over-write oldest data
			cberrs |= 2;				//	Flag buffer over-run
			return	(false);
		}
		buffer[onptr++] = element;		//	Put element on buffer. Post increment pointer
		onptr %= buffsize;
		full = (onptr == offptr);		//	Should onptr catch up with offptr buffer is 'full'
		return	(true);					//	Success
	}


	uint32_t	on_buff	() const	{	//	Return number of items on buffer
		if (full)
			return  buffsize;
		return	(offptr > onptr ? buffsize + onptr - offptr : onptr - offptr);
	}


	uint32_t	buff_space	() const	{
		return	(buffsize - on_buff())	;
	}


	uint32_t	on_buffpc	() const	{	//	Return percent full
		return	((on_buff() * 100) / buffsize);
	}


	float	on_buff_f	() const	{	//	Return normalised 0.0 to 1.0 fuel gauge
		return	((float)on_buff() / (float)buffsize);
	}

}	;	//	End of class definition


/*
 * December 2024
 * Changed SerialErrors from 1, 2, 3 etc bit numbers, to 1, 2, 4, 8 etc bit positions.
 * Allows multiple errors to be set / cleared / tested per instruction and simplifies code marginally
 */
//enum	class SerialErrors	{
enum	SerialErrors		{
	INPUT_OVERRUN	= 0b00000001,		//	bit 0
	OUTPUT_OVERRUN	= 0b00000010,		//	bit 1
	HAL_UART_RX		= 0b00000100,		//	bit 2
	HAL_TX_DMA		= 0b00001000,		//	bit 3
	HAL_UART_ERROR_CALLBACK		= 0b00010000,		//	bit 4
//	MSG_TOO_LARGE	= 0b00100000,		//	etc
//	SOME_OTHER		= 0b01000000,
	ALL				= 0b11111111,		//	Can be used to test / set / clear all
	}	;

#if 0
/**
  * @brief  UART handle Structure definition
  */
typedef struct __UART_HandleTypeDef
{
  USART_TypeDef            *Instance;                /*!< UART registers base address        */

  UART_InitTypeDef         Init;                     /*!< UART communication parameters      */

  UART_AdvFeatureInitTypeDef AdvancedInit;           /*!< UART Advanced Features initialization parameters */

  const uint8_t            *pTxBuffPtr;              /*!< Pointer to UART Tx transfer Buffer */

  uint16_t                 TxXferSize;               /*!< UART Tx Transfer size              */

  __IO uint16_t            TxXferCount;              /*!< UART Tx Transfer Counter           */

  uint8_t                  *pRxBuffPtr;              /*!< Pointer to UART Rx transfer Buffer */

  uint16_t                 RxXferSize;               /*!< UART Rx Transfer size              */

  __IO uint16_t            RxXferCount;              /*!< UART Rx Transfer Counter           */

  uint16_t                 Mask;                     /*!< UART Rx RDR register mask          */

#if defined(USART_CR1_FIFOEN)
  uint32_t                 FifoMode;                 /*!< Specifies if the FIFO mode is being used.
                                                          This parameter can be a value of @ref UARTEx_FIFO_mode. */

  uint16_t                 NbRxDataToProcess;        /*!< Number of data to process during RX ISR execution */

  uint16_t                 NbTxDataToProcess;        /*!< Number of data to process during TX ISR execution */

#endif /*USART_CR1_FIFOEN */
  __IO HAL_UART_RxTypeTypeDef ReceptionType;         /*!< Type of ongoing reception          */

  __IO HAL_UART_RxEventTypeTypeDef RxEventType;      /*!< Type of Rx Event                   */

  void (*RxISR)(struct __UART_HandleTypeDef *huart); /*!< Function pointer on Rx IRQ handler */

  void (*TxISR)(struct __UART_HandleTypeDef *huart); /*!< Function pointer on Tx IRQ handler */

  DMA_HandleTypeDef        *hdmatx;                  /*!< UART Tx DMA Handle parameters      */

  DMA_HandleTypeDef        *hdmarx;                  /*!< UART Rx DMA Handle parameters      */

  HAL_LockTypeDef           Lock;                    /*!< Locking object                     */

  __IO HAL_UART_StateTypeDef    gState;              /*!< UART state information related to global Handle management
                                                          and also related to Tx operations. This parameter
                                                          can be a value of @ref HAL_UART_StateTypeDef */

  __IO HAL_UART_StateTypeDef    RxState;             /*!< UART state information related to Rx operations. This
                                                          parameter can be a value of @ref HAL_UART_StateTypeDef */

  __IO uint32_t                 ErrorCode;           /*!< UART Error code                    */

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
  void (* TxHalfCpltCallback)(struct __UART_HandleTypeDef *huart);        /*!< UART Tx Half Complete Callback        */
  void (* TxCpltCallback)(struct __UART_HandleTypeDef *huart);            /*!< UART Tx Complete Callback             */
  void (* RxHalfCpltCallback)(struct __UART_HandleTypeDef *huart);        /*!< UART Rx Half Complete Callback        */
  void (* RxCpltCallback)(struct __UART_HandleTypeDef *huart);            /*!< UART Rx Complete Callback             */
  void (* ErrorCallback)(struct __UART_HandleTypeDef *huart);             /*!< UART Error Callback                   */
  void (* AbortCpltCallback)(struct __UART_HandleTypeDef *huart);         /*!< UART Abort Complete Callback          */
  void (* AbortTransmitCpltCallback)(struct __UART_HandleTypeDef *huart); /*!< UART Abort Transmit Complete Callback */
  void (* AbortReceiveCpltCallback)(struct __UART_HandleTypeDef *huart);  /*!< UART Abort Receive Complete Callback  */
  void (* WakeupCallback)(struct __UART_HandleTypeDef *huart);            /*!< UART Wakeup Callback                  */
#if defined(USART_CR1_FIFOEN)
  void (* RxFifoFullCallback)(struct __UART_HandleTypeDef *huart);        /*!< UART Rx Fifo Full Callback            */
  void (* TxFifoEmptyCallback)(struct __UART_HandleTypeDef *huart);       /*!< UART Tx Fifo Empty Callback           */
#endif /* USART_CR1_FIFOEN */
  void (* RxEventCallback)(struct __UART_HandleTypeDef *huart, uint16_t Pos); /*!< UART Reception Event Callback     */

  void (* MspInitCallback)(struct __UART_HandleTypeDef *huart);           /*!< UART Msp Init callback                */
  void (* MspDeInitCallback)(struct __UART_HandleTypeDef *huart);         /*!< UART Msp DeInit callback              */
#endif  /* USE_HAL_UART_REGISTER_CALLBACKS */

} UART_HandleTypeDef;
#endif

/**	class	Serial	{
 *	Handles com ports
 */
class	Serial	{

#define		MAX_NUMOF_UARTS		4

//#define		LIVE_TXBUFF_SIZE	500	//	This many chars max passed per DMA Transmit
#define		DEFAULT_LIN_INBUFF_SIZE		200		//	Long enough for longest command line
#define		DEFAULT_RING_OUTBUFF_SIZE	200		//	Two buffers this size, lin and ring, large as possible but not silly - buffer overrun issue resolved !

	UART_HandleTypeDef * const m_huartn	;		//	Which hardware usart
	const		size_t	tx_ringbuff_size = DEFAULT_RING_OUTBUFF_SIZE;	//	Can get overridden in constructor
	const		size_t	rx_ringbuff_size = DEFAULT_LIN_INBUFF_SIZE;		//	Can get overridden in constructor
	volatile	bool	tx_busy 		{ false };

	char	*	lin_inbuff	;//	[DEFAULT_LIN_INBUFF_SIZE + 4] 	{0};	//	Command line, not circular buffer
	char	*	lin_tx_buff ;	//	pointer to linear transmit buffer for passing to DMA Tx
	uint32_t	time_ms_of_most_recent_rx { 0 }	;	//	added 19th Aug 2025 - see uint32_t	when() - reports uwTick when most recent rx char arrived

	uint8_t		ch[4] {0};
	uint32_t	lin_inbuff_onptr 	{0L};

	uint32_t	in_esc_seq	{ 0 };

	uint32_t	serial_error 		{0L};
	uint8_t		rxbuff	[4] {0};			//	Rx DMA places 1 byte here. ISR copies this to circular buffer

	CircularBuffer	<uint8_t>	RxCBuff	{rx_ringbuff_size}	;	//	Default defined fully above, may be overridden in constructor
	CircularBuffer	<uint8_t>	TxCBuff	{tx_ringbuff_size}	;	//	Default defined fully above, may be overridden in constructor - NEW Aug 2026

public:

	void	Constructor_Core	()	;

	~Serial	()	{
		delete	lin_inbuff;
		delete	lin_tx_buff;
	}

	Serial	(UART_HandleTypeDef &which_port)	;	//	:	m_huartn {&which_port}
	Serial	(UART_HandleTypeDef &which_port, const size_t tx_buffsize)	;	//	:	m_huartn {&which_port}
	Serial	(UART_HandleTypeDef &which_port, const size_t tx_buffsize, const size_t rx_buffsize)	;	//	:	m_huartn {&which_port}

	char *	test_for_rx_message	();	//	Returns buffer address on receiving '\r', presumably at end of command string, NULL otherwise
	bool	write	(const uint8_t * t, int32_t len)	;	//	Puts all on buffer. Transmit only once per ms
	bool	write	(const char * t, int32_t len)	;
	bool	tx_any_buffered	()	;	//	Call this every 1ms to see if sending complete and send more if there is
	void	rx_intrpt_handler_core_DMA	();
	bool	start_rx	()	;		//	Call from startup. Call from constructor is too early - CHECK!! True, but why?
	void	set_tx_busy	(bool true_or_false)	;
	bool	get_tx_busy	()	;		//	Returns true when DMA in progress
//	bool	test_error	(uint32_t mask)	const;	//	returns true if error bits set after masking
	uint32_t	test_error	(uint32_t mask)	;	//	returns true if error bits set after masking
	uint32_t	clear_error	(uint32_t bit);	//	-1 clears all error bits, 0 clears none, INPUT_OVERRUN_ERROR clears INPUT_OVERRUN_ERROR. Returns serial_error
	void	set_error	(uint32_t bit); //	{	serial_error |= (1 << bit);	}
	void	report_error	();		//	writes to 'serial' port errors if any and resets all errors

	bool	get	(uint8_t & a)	;
	bool	read	(uint8_t * buff, size_t & len)	;
	uint32_t	when	()	{	return	(time_ms_of_most_recent_rx)		;	}
	const	size_t	rx_buffer_size	()	{	return	(rx_ringbuff_size)	;	}	//	Useful to prevent read beyond end of buffer
	const	size_t	tx_buffer_size	()	{	return	(tx_ringbuff_size)	;	}	//	Probably not useful

}	;


#endif /* INC_SERIAL_HPP_ */
