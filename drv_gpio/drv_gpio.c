/* Copyright (c) Nordic Semiconductor ASA
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 * 
 *   3. Neither the name of Nordic Semiconductor ASA nor the names of other
 *   contributors to this software may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 * 
 *   4. This software must only be used in a processor manufactured by Nordic
 *   Semiconductor ASA, or in a processor manufactured by a third party that
 *   is used in combination with a processor manufactured by Nordic Semiconductor.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "drv_gpio.h"
#include "nrf_error.h"

#include <stdbool.h>
#include <stdlib.h>

typedef struct
{
    uint8_t current_sense  : DRV_GPIO_SENSE_Width;
    uint8_t handler_enable : DRV_GPIO_HANDLER_Width;
    uint8_t rfu0           : sizeof(uint8_t) * 8 - 
    (
        DRV_GPIO_LEVEL_Width   + 
        DRV_GPIO_SENSE_Width   + 
        DRV_GPIO_HANDLER_Width
    );
} m_drv_gpio_pin_state;


static struct
{
    struct
    {
        uint32_t    pin_msk;
        uint8_t     cnt;
    } gpiote;
    m_drv_gpio_pin_state    gpio_states[DRV_GPIO_NR_OF_PINS];
    drv_gpio_sig_handler_t  sig_handler;
} m_drv_gpio = {.gpiote.cnt = 0, .gpiote.pin_msk = 0, .sig_handler = NULL};


#if DRV_GPIO_SENSE_NONE != GPIOTE_CONFIG_POLARITY_None
#error "ERROR: DRV_GPIO_SENSE_NONE != GPIOTE_CONFIG_POLARITY_None."
#endif
#if DRV_GPIO_SENSE_LOTOHI != GPIOTE_CONFIG_POLARITY_LoToHi
#error "ERROR: DRV_GPIO_SENSE_LOTOHI != GPIOTE_CONFIG_POLARITY_LoToHi."
#endif
#if DRV_GPIO_SENSE_HITOLO != GPIOTE_CONFIG_POLARITY_HiToLo
#error "ERROR: DRV_GPIO_SENSE_HITOLO != GPIOTE_CONFIG_POLARITY_HiToLo."
#endif
#if DRV_GPIO_SENSE_ANY != GPIOTE_CONFIG_POLARITY_Toggle
#error "ERROR: DRV_GPIO_SENSE_ANY != GPIOTE_CONFIG_POLARITY_Toggle."
#endif


#if DRV_GPIO_PULL_NONE != GPIO_PIN_CNF_PULL_Disabled
#error "ERROR: DRV_GPIO_PULL_NONE != GPIO_PIN_CNF_PULL_Disabled."
#endif
#if DRV_GPIO_PULL_UP != GPIO_PIN_CNF_PULL_Pullup
#error "ERROR: DRV_GPIO_PULL_UP != GPIO_PIN_CNF_PULL_Pullup."
#endif
#if DRV_GPIO_PULL_DOWN != GPIO_PIN_CNF_PULL_Pulldown
#error "ERROR: DRV_GPIO_PULL_DOWN != GPIO_PIN_CNF_PULL_Pulldown."
#endif


#if DRV_GPIO_LEVEL_LOW != GPIOTE_CONFIG_OUTINIT_Low
#error "ERROR: DRV_GPIO_LEVEL_LOW != GPIOTE_CONFIG_OUTINIT_Low."
#endif
#if DRV_GPIO_LEVEL_HIGH != GPIOTE_CONFIG_OUTINIT_High
#error "ERROR: DRV_GPIO_LEVEL_HIGH != GPIOTE_CONFIG_OUTINIT_High."
#endif


#if DRV_GPIO_DRIVE_S0S1 != GPIO_PIN_CNF_DRIVE_S0S1
#error "ERROR: DRV_GPIO_DRIVE_S0S1 != GPIO_PIN_CNF_DRIVE_S0S1."
#endif
#if DRV_GPIO_DRIVE_H0S1 != GPIO_PIN_CNF_DRIVE_H0S1
#error "ERROR: DRV_GPIO_DRIVE_H0S1 != GPIO_PIN_CNF_DRIVE_H0S1."
#endif
#if DRV_GPIO_DRIVE_S0H1 != GPIO_PIN_CNF_DRIVE_S0H1
#error "ERROR: DRV_GPIO_DRIVE_S0H1 != GPIO_PIN_CNF_DRIVE_S0H1."
#endif
#if DRV_GPIO_DRIVE_H0H1 != GPIO_PIN_CNF_DRIVE_H0H1
#error "ERROR: DRV_GPIO_DRIVE_H0H1 != GPIO_PIN_CNF_DRIVE_H0H1."
#endif
#if DRV_GPIO_DRIVE_D0S1 != GPIO_PIN_CNF_DRIVE_D0S1
#error "ERROR: DRV_GPIO_DRIVE_D0S1 != GPIO_PIN_CNF_DRIVE_D0S1."
#endif
#if DRV_GPIO_DRIVE_D0H1 != GPIO_PIN_CNF_DRIVE_D0H1
#error "ERROR: DRV_GPIO_DRIVE_D0H1 != GPIO_PIN_CNF_DRIVE_D0H1."
#endif
#if DRV_GPIO_DRIVE_S0D1 != GPIO_PIN_CNF_DRIVE_S0D1
#error "ERROR: DRV_GPIO_DRIVE_S0D1 != GPIO_PIN_CNF_DRIVE_S0D1."
#endif
#if DRV_GPIO_DRIVE_H0D1 != GPIO_PIN_CNF_DRIVE_H0D1
#error "ERROR: DRV_GPIO_DRIVE_H0D1 != GPIO_PIN_CNF_DRIVE_H0D1."
#endif


static __INLINE bool m_gpiote_pin_add(uint8_t pin)
{
    if ( ((m_drv_gpio.gpiote.pin_msk & (1UL << pin)) == 0)
    &&   (m_drv_gpio.gpiote.cnt                       < DRV_GPIO_NR_OF_GPIOTE_INSTANCES) )
    {
        for ( uint8_t i = 0; i < DRV_GPIO_NR_OF_GPIOTE_INSTANCES; i++ )
        {
            if ( ((NRF_GPIOTE->CONFIG[i] & GPIOTE_CONFIG_MODE_Msk) >> GPIOTE_CONFIG_MODE_Pos) == GPIOTE_CONFIG_MODE_Disabled )
            {
                NRF_GPIOTE->CONFIG[i] = (NRF_GPIOTE->CONFIG[i] & ~GPIOTE_CONFIG_PSEL_Msk) | (pin << GPIOTE_CONFIG_PSEL_Pos);
                
                m_drv_gpio.gpiote.pin_msk |= (1UL << pin);
                ++m_drv_gpio.gpiote.cnt;
                
                return ( true );
            }
        }
    }
    
    return ( false );
}


static __INLINE bool m_gpiote_pin_remove(uint8_t pin)
{
    if ( ((m_drv_gpio.gpiote.pin_msk & (1UL << pin)) != 0 )
    &&   (m_drv_gpio.gpiote.cnt                      >  0) )
    {
        for ( uint8_t i = 0; i < DRV_GPIO_NR_OF_GPIOTE_INSTANCES; i++ )
        {
            if ( ((NRF_GPIOTE->CONFIG[i] & GPIOTE_CONFIG_MODE_Msk) >> GPIOTE_CONFIG_MODE_Pos) != GPIOTE_CONFIG_MODE_Disabled )
            {
                NRF_GPIOTE->INTENCLR = GPIOTE_INTENCLR_IN0_Clear << (GPIOTE_INTENCLR_IN0_Pos + i);
                
                NRF_GPIOTE->CONFIG[i] = (NRF_GPIOTE->CONFIG[i] & ~GPIOTE_CONFIG_MODE_Msk) | (GPIOTE_CONFIG_MODE_Disabled << GPIOTE_CONFIG_MODE_Pos);
                
                m_drv_gpio.gpiote.pin_msk &= ~(1UL << pin);
                --m_drv_gpio.gpiote.cnt;
                
                return ( true );
            }
        }
    }
    
    return ( false );
}


static __INLINE uint8_t m_gpiote_idx_get(uint8_t pin)
{
    uint8_t i;
    
    for ( i = 0; i < DRV_GPIO_NR_OF_GPIOTE_INSTANCES; i++ )
    {
        if ( (((NRF_GPIOTE->CONFIG[i]    & GPIOTE_CONFIG_PSEL_Msk) >> GPIOTE_CONFIG_PSEL_Pos) == pin)
        &&   ((m_drv_gpio.gpiote.pin_msk & (1UL << pin))                                      != 0) )
        {
            return ( i );
        }
    }
    
    return ( i );
}


void drv_gpio_sig_handler_set(drv_gpio_sig_handler_t sig_handler)
{
    m_drv_gpio.sig_handler = sig_handler;
}


uint32_t drv_gpio_inpin_cfg(uint8_t pin, drv_gpio_inpin_cfg_t cfg, uint32_t ** p_event)
{   
    if ( pin >= DRV_GPIO_NR_OF_PINS )
    {
        return ( NRF_ERROR_INVALID_PARAM );
    }
    
    if ( cfg.gpiote == DRV_GPIO_GPIOTE_ENABLE )
    {
        if ( m_gpiote_pin_add(pin) )
        {
            uint8_t const idx = m_gpiote_idx_get(pin);

            m_drv_gpio.gpio_states[pin].handler_enable = cfg.handler;
            
            NRF_GPIO->PIN_CNF[pin] =
            (
                (GPIO_PIN_CNF_DIR_Input     << GPIO_PIN_CNF_DIR_Pos) |
                (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                (cfg.pull                   << GPIO_PIN_CNF_PULL_Pos)
            );
            
            NRF_GPIOTE->CONFIG[idx] =
            (
                (GPIOTE_CONFIG_MODE_Event   << GPIOTE_CONFIG_MODE_Pos) |
                (pin                        << GPIOTE_CONFIG_PSEL_Pos) |
                (cfg.sense                  << GPIOTE_CONFIG_POLARITY_Pos)
            );

            if ( p_event != NULL )
            {
                *p_event = (uint32_t *)&(NRF_GPIOTE->EVENTS_IN[idx]);
            }
            
            if ( cfg.handler == DRV_GPIO_HANDLER_ENABLE )
            {
                NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_IN0_Set << (GPIOTE_INTENSET_IN0_Pos + idx);
            }
        }
        else
        {
            return ( NRF_ERROR_NOT_FOUND );
        }
    }
    else
    {
       (void)m_gpiote_pin_remove(pin);
        
        m_drv_gpio.gpio_states[pin].current_sense = cfg.sense;
        m_drv_gpio.gpio_states[pin].handler_enable = cfg.handler;
        
        NRF_GPIO->PIN_CNF[pin] =
        (
            (GPIO_PIN_CNF_DIR_Input     << GPIO_PIN_CNF_DIR_Pos) |
            (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
            (cfg.pull                   << GPIO_PIN_CNF_PULL_Pos)
        );

        if ( cfg.sense != DRV_GPIO_SENSE_NONE )
        {
            uint8_t level;
            
            if (cfg.pull == DRV_GPIO_PULL_NONE)
            {
                drv_gpio_inpin_get(pin, &level);
            }
            else
            {
                level = ( cfg.pull == DRV_GPIO_PULL_UP ) ? DRV_GPIO_LEVEL_HIGH : DRV_GPIO_LEVEL_LOW;
            }
            
            NRF_GPIO->DETECTMODE = GPIO_DETECTMODE_DETECTMODE_LDETECT << GPIO_DETECTMODE_DETECTMODE_Pos;
            NRF_GPIO->LATCH = (1UL << pin);
            
            if ( level == DRV_GPIO_LEVEL_LOW )
            {
                NRF_GPIO->PIN_CNF[pin] |= GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos;
            }
            else
            {
                NRF_GPIO->PIN_CNF[pin] |= GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos;
            }
            
            if ( cfg.handler == DRV_GPIO_HANDLER_ENABLE )
            {
                NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Set << GPIOTE_INTENSET_PORT_Pos;
            }
        }
    }
    
    return ( NRF_SUCCESS );
}


uint32_t drv_gpio_inpins_cfg(uint32_t pin_msk, drv_gpio_inpin_cfg_t cfg, uint32_t ** p_event_arr)
{
    uint32_t tmp_u32    = pin_msk;
    uint8_t  i          = 0;
    uint8_t  n          = 0;
    
    while ( tmp_u32 > 0 )
    {
        if ( (tmp_u32 & 1) != 0 )
        {
            uint32_t ret_val = ( p_event_arr == NULL ) ? drv_gpio_inpin_cfg(i, cfg, NULL) : drv_gpio_inpin_cfg(i, cfg, &(p_event_arr[n]));
                
            if ( ret_val != NRF_SUCCESS )
            {
                return ( ret_val );
            }
            ++n;
        }
        
        tmp_u32 >>= 1;
        ++i;
    }
    
    return ( NRF_SUCCESS );
}


uint32_t drv_gpio_outpin_cfg(uint8_t pin, drv_gpio_outpin_cfg_t cfg, uint32_t ** p_task)
{
    if ( pin >= DRV_GPIO_NR_OF_PINS )
    {
        return ( NRF_ERROR_INVALID_PARAM );
    }

    if ( cfg.gpiote == DRV_GPIO_GPIOTE_ENABLE )
    {
        if ( m_gpiote_pin_add(pin) )
        {
            uint8_t     const   idx     = m_gpiote_idx_get(pin);
            uint32_t            config  = 
            (
                (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
                (pin                     << GPIOTE_CONFIG_PSEL_Pos) |
                (cfg.level               << GPIOTE_CONFIG_OUTINIT_Pos)
            );
            
            if ( p_task != NULL )
            {
                uint32_t *p_task_addr;

                switch ( cfg.task )
                {
                    case DRV_GPIO_TASK_CLEAR:
                        p_task_addr = (uint32_t *)&(NRF_GPIOTE->TASKS_CLR[idx]);
                        break;
                    case DRV_GPIO_TASK_SET:
                        p_task_addr = (uint32_t *)&(NRF_GPIOTE->TASKS_SET[idx]);
                        break;
                    case DRV_GPIO_TASK_TOGGLE:
                        config |= GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos;
                        p_task_addr = (uint32_t *)&(NRF_GPIOTE->TASKS_OUT[idx]);
                        break;
                }
                
                *p_task = p_task_addr;
            }
            
            NRF_GPIOTE->CONFIG[idx] = config;
        }
        else
        {
            return ( NRF_ERROR_NOT_FOUND );
        }
    }
    else
    {
       (void)m_gpiote_pin_remove(pin);
        
        NRF_GPIO->PIN_CNF[pin] =
        (
            (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)   |
            (cfg.drive               << GPIO_PIN_CNF_DRIVE_Pos)
        );
    }
    
    return ( NRF_SUCCESS );
}


uint32_t drv_gpio_outpins_cfg(uint32_t pin_msk, drv_gpio_outpin_cfg_t cfg, uint32_t ** p_task_arr)
{
    uint32_t tmp_u32 = pin_msk;
    uint8_t  i = 0;
    uint8_t  n = 0;
    
    while ( tmp_u32 > 0 )
    {
        if ( (tmp_u32 & 1) != 0 )
        {
            uint32_t ret_val = ( p_task_arr == NULL ) ? drv_gpio_outpin_cfg(i, cfg, NULL) : drv_gpio_outpin_cfg(i, cfg, &(p_task_arr[n]));
                
            if ( ret_val != NRF_SUCCESS )
            {
                return ( ret_val );
            }
            ++n;
        }
        
        tmp_u32 >>= 1;
        ++i;
    }
    
    return ( NRF_SUCCESS );
}


uint32_t drv_gpio_pin_disconnect(uint8_t pin)
{
    if ( pin >= DRV_GPIO_NR_OF_PINS )
    {
        return ( NRF_ERROR_INVALID_PARAM );
    }
    
   (void)m_gpiote_pin_remove(pin);
    
    NRF_GPIO->PIN_CNF[pin] =
    (
        (GPIO_PIN_CNF_DIR_Input        << GPIO_PIN_CNF_DIR_Pos)   |
        (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
    );
    
    return ( NRF_SUCCESS );
}


uint32_t drv_gpio_pins_disconnect(uint32_t pin_msk)
{
    uint32_t tmp_u32 = pin_msk;
    uint8_t  i       = 0;
    
    if ( pin_msk == 0 )
    {
        return ( NRF_ERROR_INVALID_PARAM );
    }
    
    while ( tmp_u32 > 0 )
    {
        if ( (tmp_u32 & 1) != 0 )
        {
            (void)drv_gpio_pin_disconnect(i);
        }
        
        tmp_u32 >>= 1;
        ++i;
    }
    
    return ( NRF_SUCCESS );
}


uint32_t drv_gpio_inpin_get(uint8_t pin, uint8_t *p_level)
{
    if ( (pin     >= DRV_GPIO_NR_OF_PINS)
    &&   (p_level == NULL) )
    {
        return ( NRF_ERROR_INVALID_PARAM );
    }

    *p_level = ( (NRF_GPIO->IN & (1UL << pin)) != 0 ) ? 1 : 0;
    
    return ( NRF_SUCCESS );
}


uint32_t drv_gpio_inport_get(void)
{
    return ( NRF_GPIO->IN );
}


static void m_gpiote_outport_modify(uint32_t high_msk, uint32_t low_msk)
{
    uint32_t high_msk_gpiote = high_msk        & m_drv_gpio.gpiote.pin_msk;
    uint32_t low_msk_gpiote  = low_msk         & m_drv_gpio.gpiote.pin_msk;
    uint32_t tmp_u32         = high_msk_gpiote | low_msk_gpiote;
    
    if ( (high_msk_gpiote & low_msk_gpiote) == 0 )
    {
        uint8_t  i = 0;
        
        while ( tmp_u32 > 0 )
        {
            if ( (tmp_u32 & 1) != 0 )
            {
                uint8_t idx = m_gpiote_idx_get(i);
                
                if ( (high_msk_gpiote & (1UL << i)) != 0 )
                {
                    NRF_GPIOTE->TASKS_SET[idx] = 1;
                }
                if ( (low_msk_gpiote & (1UL << i)) != 0 )
                {
                    NRF_GPIOTE->TASKS_CLR[idx] = 1;
                }
            }
            
            tmp_u32 >>= 1;
            ++i;
        }
    }
}


uint32_t drv_gpio_outpin_level_set(uint8_t pin, uint8_t level)
{
    uint32_t const pin_msk = (1UL << pin);
    
    if ( (pin   >= DRV_GPIO_NR_OF_PINS)
    &&   (level >  DRV_GPIO_LEVEL_HIGH) )
    {
        return ( NRF_ERROR_INVALID_PARAM );
    }
    
    if ( (pin_msk & m_drv_gpio.gpiote.pin_msk) != 0 )
    {
        if ( level == DRV_GPIO_LEVEL_LOW )
        {
            m_gpiote_outport_modify(0, pin_msk);
        }
        else
        {
            m_gpiote_outport_modify(pin_msk, 0);
        }
    }
    else
    {
        if ( level == DRV_GPIO_LEVEL_LOW )
        {
            NRF_GPIO->OUTCLR = pin_msk;
        }
        else
        {
            NRF_GPIO->OUTSET = pin_msk;
        }
    }

    return ( NRF_SUCCESS );
}


uint32_t drv_gpio_outport_modify(uint32_t high_msk, uint32_t low_msk)
{
    if ( (high_msk & low_msk) != 0 )
    {
        return ( NRF_ERROR_INVALID_PARAM );
    }
    
    m_gpiote_outport_modify(high_msk, low_msk);
    
    NRF_GPIO->OUTSET = high_msk & ~m_drv_gpio.gpiote.pin_msk;
    NRF_GPIO->OUTCLR = low_msk  & ~m_drv_gpio.gpiote.pin_msk;
    
    return ( NRF_SUCCESS );
}


void drv_gpio_outport_toggle(uint32_t toggle_msk)
{
    if ( toggle_msk != 0 )
    {
        uint32_t set_bits = (toggle_msk ^ NRF_GPIO->IN) & toggle_msk;
        uint32_t clr_bits = ~set_bits & toggle_msk;
        uint32_t set_bits_gpio   = set_bits & ~m_drv_gpio.gpiote.pin_msk;
        uint32_t clr_bits_gpio   = clr_bits & ~m_drv_gpio.gpiote.pin_msk;
        uint32_t set_bits_gpiote = set_bits &  m_drv_gpio.gpiote.pin_msk;
        uint32_t clr_bits_gpiote = clr_bits &  m_drv_gpio.gpiote.pin_msk;
        
        
        if ( (set_bits_gpiote | clr_bits_gpiote ) != 0 )
        {
            m_gpiote_outport_modify(set_bits_gpiote, clr_bits_gpiote);
        }

        if ( (set_bits_gpio | clr_bits_gpio ) != 0 )
        {
            NRF_GPIO->OUTSET = set_bits_gpio;
            NRF_GPIO->OUTCLR = clr_bits_gpio;
        }
    }
}


void drv_gpio_outport_set(uint32_t outport)
{
    if ( m_drv_gpio.gpiote.pin_msk == 0 )
    {
        NRF_GPIO->OUT = outport;
    }
    else
    {
        drv_gpio_outport_modify(outport, ~outport);
    }
}


static void m_pin_event_report(uint8_t pin, uint8_t sense_edge)
{
    if ( (m_drv_gpio.gpio_states[pin].handler_enable == DRV_GPIO_HANDLER_ENABLE)
    &&   (m_drv_gpio.sig_handler                     != NULL) )
    {
        m_drv_gpio.sig_handler(pin, sense_edge);
    }
}


void GPIOTE_IRQHandler(void)
{
    for ( uint_fast8_t i = 0; i < DRV_GPIO_NR_OF_GPIOTE_INSTANCES; ++i )
    {
        if ( (NRF_GPIOTE->EVENTS_IN[i]                                                            != 0) 
        &&   ((NRF_GPIOTE->INTENSET & (GPIOTE_INTENSET_IN0_Set << (GPIOTE_INTENSET_IN0_Pos + i))) != 0) )
        {
            uint8_t const pin           = (NRF_GPIOTE->CONFIG[i] & GPIOTE_CONFIG_PSEL_Msk)     >> GPIOTE_CONFIG_PSEL_Pos;
            uint8_t const sensed_edge   = (NRF_GPIOTE->CONFIG[i] & GPIOTE_CONFIG_POLARITY_Msk) >> GPIOTE_CONFIG_POLARITY_Pos;
        
            NRF_GPIOTE->EVENTS_IN[i] = 0;
            (void)NRF_GPIOTE->EVENTS_IN[i];
            
            m_pin_event_report(pin, sensed_edge);
        }
    }
    
    while ( NRF_GPIOTE->EVENTS_PORT != 0 )
    {
        for ( uint_fast8_t i = 0; i < DRV_GPIO_NR_OF_PINS; ++i )
        {
            if ( (m_drv_gpio.gpio_states[i].current_sense != DRV_GPIO_SENSE_NONE)
            &&   ((NRF_GPIO->LATCH & (1UL << i))          != 0) )
            {
                uint8_t const sense_level_at_entry = (((NRF_GPIO->PIN_CNF[i] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos) == GPIO_PIN_CNF_SENSE_High) ? DRV_GPIO_LEVEL_HIGH : DRV_GPIO_LEVEL_LOW; 
                uint8_t const current_pin_sense    = m_drv_gpio.gpio_states[i].current_sense;
                
                do
                {
                    if ( ((NRF_GPIO->PIN_CNF[i] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos) == GPIO_PIN_CNF_SENSE_Low )
                    {
                        NRF_GPIO->PIN_CNF[i] = (NRF_GPIO->PIN_CNF[i] & ~GPIO_PIN_CNF_SENSE_Msk) | (GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos);
                    }
                    else
                    {
                        NRF_GPIO->PIN_CNF[i] = (NRF_GPIO->PIN_CNF[i] & ~GPIO_PIN_CNF_SENSE_Msk) | (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);
                    }
                    NRF_GPIO->LATCH = 1UL << i;
                    (void)NRF_GPIO->LATCH;
                }
                while ( (NRF_GPIO->LATCH & (1UL << i)) != 0 );
             
                NRF_GPIOTE->EVENTS_PORT = 0;
               (void)NRF_GPIOTE->EVENTS_PORT;
                
                if ( ( (current_pin_sense == DRV_GPIO_SENSE_LOTOHI)
                  ||   (current_pin_sense == DRV_GPIO_SENSE_ANY)
                     )
                &&   (sense_level_at_entry == DRV_GPIO_LEVEL_HIGH) )
                {
                    m_pin_event_report(i, DRV_GPIO_SENSE_LOTOHI);
                }
                else if ( ( (current_pin_sense == DRV_GPIO_SENSE_HITOLO)
                       ||   (current_pin_sense == DRV_GPIO_SENSE_ANY)
                          )
                &&        (sense_level_at_entry == DRV_GPIO_LEVEL_LOW) )
                {
                    m_pin_event_report(i, DRV_GPIO_SENSE_HITOLO);
                }
            }
        }
    }
}