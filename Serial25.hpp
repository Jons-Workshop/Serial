#pragma once
/*
 * Serial25.hpp
 *
 *  Created on: Jun 29, 2025
 *      Author: jon34
 */

#ifndef INC_SERIAL25_HPP_
#define INC_SERIAL25_HPP_

/**	class CircularBuffer
 * Jon Freeman
 * 25th Feb 2025
 *
 *	Comment only changed 20th June 2025
 *
 *	Uses dynamic allocation so that buffer size can be brought as a parameter
 *	All code to implement CircularBuffer is in this file (no corresponding .cpp)
 *
 *	Memory allocated using 'nothrow'.
 *	This is as circular buffers are likely setup as part of any error reporting code.
 *	It is up to the user to check bit 0 of 'errors' to checkbuff_start != nullptr;
 */

#include	<new>	//	needed to handle std::nothrow

template <class T>	class	CircularBuffer	{	//	Class to manage circular buffer of type T objects
//	const	size_t	buffsize;					//	Initialised by parameter 's' brought to Constructor
	const	size_t	buffsize=100;					//	Initialised by parameter 's' brought to Constructor
	T *	buffer = new (std::nothrow) T[buffsize] { 0 };	//	Allocate and initialise buffer space if possible
	bool	full	{ false };
	size_t	onptr	{ 0L };
	size_t	offptr	{ 0L };
	size_t	errors	{ 0L };	//	low order bits set by various errors - in progress

public:

	CircularBuffer	()	{
        if (buffer == nullptr) {
            errors = 1;				//	Flag fatal buffer allocation failure
        }
	}                          // Default constructor
	CircularBuffer	(const size_t s) : buffsize { s }	{
        if (buffer == nullptr) {
            errors = 1;				//	Flag fatal buffer allocation failure
        }
	}	//	Parameter is buffer size
	~CircularBuffer	()	{	delete buffer;	}   // Destructor

//	bool	fail	()	{	return	(buffer == nullptr);	}

	size_t	get_errors	()	{	return	(errors);	}
	void	clr_errors	()	{	errors = 0;	}

	bool	empty	()	{	return	(!full && (onptr == offptr));	}
//	bool	available()	{	return	(!empty());	}

	bool	get		(T & element)	{	//	Returns false on buffer empty fail
		if	(empty())					//	Stores 'got' element at parameter address supplied
			return	(false);
		element = buffer[offptr++];		//	Get element from buffer. Post increment pointer
		offptr %= buffsize;
		full = false;					//	Have removed one element. Therefore can not be 'full'
		return	(true);					//	Success
	}

	bool	put		(T & element)	{	//	Returns false on buffer full fail
		if	(full)	{					//	Does not over-write oldest data
			errors |= 2;				//	Flag buffer over-run
			return	(false);
		}
		buffer[onptr++] = element;		//	Put element on buffer. Post increment pointer
		onptr %= buffsize;
		full = (onptr == offptr);		//	Should onptr catch up with offptr buffer is 'full'
		return	(true);					//	Success
	}

	size_t	on_buff	()	{	//	Return number of items on buffer
		if (full)
			return  buffsize;
		int32_t	rv = onptr - offptr;
		if	(rv < 0)
			rv += buffsize;
		return	(rv);
	}

	size_t	on_buffpc	()	{	//	Return percent full
		return	((on_buff() * 100) / buffsize);
	}

	float	on_buff_f	()	{	//	Return normalised 0.0 to 1.0 fuel gauge
		return	((float)on_buff() / (float)buffsize);
	}

}	;	//	End of class definition


/*
 * Jon Freeman		27th Feb 2025
 *
 * DualCircularBuffer contains circular buffers intended for use in uart Tx and Rx
 * Usage :
 * 			DualCircularBuffer	MyDualCircBuff	(txbuffsize, rxbuffsize);
 *
 *	Two linear arrays are also created, 'dma_buffer' of txbuffsize, and 'cmd_line' of rxbuffsize.
 *	For transmitting data, user code uses 'write(T*source, size_t len)', data elements may be any valid value of type T
 *	For receiving, uart places elements on Rx circuar buffer.
 *		User code uses 'getcmdline(T*dest)' to move elements from circ buff to linear command line buff
 */

//#include	"circbuff.hpp"	//	Contains entire circular buffer code

template <class T> class DualCircularBuffer	{	//	class containing two CircularBuffers
	const	size_t	txbuffsize;
	const	size_t	rxbuffsize;
	CircularBuffer<T>	Tx	;	//	Two private circular buffers
	CircularBuffer<T>	Rx	;	//	Defined fully in "circbuff.hpp", no .cpp code
	T *	dma_buffer 	= new (std::nothrow) T[txbuffsize] { 0 };	//	Using DMA Tx, copy from circbuff into this linear buffer
	T *	cmd_line 	= new (std::nothrow) T[rxbuffsize] { 0 };	//	Using DMA Tx, copy from circbuff into this linear buffer
	size_t	errors		{ 0L };	//	low order bits set by various errors - in progress
	size_t	cmd_offset	{ 0L };	//	offset into 'cmd_line'

public:

	DualCircularBuffer	(const size_t tx_buffsize, const size_t rx_buffsize)	//	Constructor
        : txbuffsize { tx_buffsize }
		, rxbuffsize { rx_buffsize }
        , Tx	{txbuffsize}	//	Initialise Tx buffer with size
        , Rx	{rxbuffsize}	//
    {
//        cout << "In DualCircBuff Constructor with " << Tx.get_errors() << ", " << Rx.get_errors() << endl;
        if (dma_buffer == nullptr) {
            errors = 0x000101;				//	Flag fatal buffer allocation failure
        }
    }

    ~DualCircularBuffer	()	{		}	//	destructor

    size_t	get_errors	()	{	//	Rx errors bits 0-7, Tx errors bits 8-15
        return	((Tx.get_errors() << 8) | Rx.get_errors() | errors);
    }

    void	clr_errors	()	{	Tx.reset_errors();	Rx.reset_errors();	errors = 0L; }

/*  bool	write	(T * source, size_t len)	{
 *		source is address of element or element array to put on buffer
 *		len is length to write
 *		If sufficient space available on circular buffer, all elements are copied, write returns 'true'.
 *		If insufficient space available on circular buffer, elements are copied until buffer full, write returns 'false'.
*/
    bool	write	(T * source, size_t len)		{	//	Put stuff on Tx buffer, length to send len
    	bool	rv { false } ;
        int	i { 0 } ;
//    	cout << "In write, sending [" << source << "]" << endl;
        while (i < len && (rv = Tx.put(source[i++])))	{        }
        return	(rv);	//	true on success, false on Tx.put buffer overflow fail
    }


/*  bool	read	(T * dest, size_t & len)	{
 *		dest is address of buffer to put elements into
 *		On arrival len is the requested max numof elements to be read.
 *		If fewer than len elements on buffer, read will read all, update len accordingly and return false
 *		If same or more elements on buffer, read reads initial len elements, and returns true.
 *	Be sure to understand return of 'false' meaning read has emptied buffer.
*/
    bool	read	(T * dest, size_t & len)	{	//	Get stuff from Rx buffer, return count in len
    	bool	rv { false } ;
    	int	i { 0 } ;
        while (i < len && (rv = Rx.get(dest[i])))	{
        	i++;
        }
        len = i;
//    	cout << "In read, got char count [" << len << "]" << endl;
        return	(rv);
    }



    void	loopback	()	{					//	Move stuff from Tx buffer to Rx buffer to test
//    	int 	cnt { 0 };
    	char	ch;
        while (Tx.get(ch))	{
//        	cout << '[' << ch << ']';
        	Rx.put(ch);
//            cnt++;
        }
//		cout << "Loopback shifted count = " << cnt << endl;
    }


    void	put_one_from_uart	(char c)	{	//	Take a char from uart and put on buffer
    	Rx.put((T)c);
    }

    bool	get_one_for_uart	(char & c)	{
    	T ch = (T)c;
    	return	(Tx.get(c));
    }

    bool	get_string_for_uart	(T * dest, size_t & len)	{	//	Get stuff from Rx buffer, return count in len
    	bool	rv { false } ;
    	int	i { 0 } ;
        while (i < len && (rv = Tx.get(dest[i])))	{
        	i++;
        }
        len = i;
//    	cout << "In read, got char count [" << len << "]" << endl;
        return	(rv);
    }
};









#endif /* INC_SERIAL25_HPP_ */
