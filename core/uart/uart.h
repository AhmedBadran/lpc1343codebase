/**************************************************************************/
/*! 
    @file     uart.h
    @author   K. Townsend (microBuilder.eu)
    @date     22 March 2010
    @version  0.10

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2010, microBuilder SARL
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/

#ifndef __UART_H__ 
#define __UART_H__

#include "projectconfig.h"

// Buffer used for circular fifo
typedef struct _uart_buffer_t
{
  uint8_t ep_dir;
  volatile uint8_t len;
  volatile uint8_t wr_ptr;
  volatile uint8_t rd_ptr;
  uint8_t buf[CFG_UART_BUFSIZE];
} uart_buffer_t;

// UART Protocol control block
typedef struct _uart_pcb_t
{
  BOOL initialised;
  uint32_t baudrate;
  uint32_t status;
  uint32_t pending_tx_data;
  uart_buffer_t rxfifo;
} uart_pcb_t;

void UART_IRQHandler(void);
uart_pcb_t *uartGetPCB();
void uartInit(uint32_t Baudrate);
void uartSend(uint8_t *BufferPtr, uint32_t Length);
void uartSendByte (uint8_t byte);

// Rx Buffer access control
void uartRxBufferInit();
uint8_t uartRxBufferRead();
void uartRxBufferWrite(uint8_t data);
void uartRxBufferClearFIFO();
uint8_t uartRxBufferDataPending();
bool uartRxBufferReadArray(byte_t* rx, size_t* len);

#endif
