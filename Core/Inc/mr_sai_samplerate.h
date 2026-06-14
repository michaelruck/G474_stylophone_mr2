/**
  ******************************************************************************
  * @file           : mr_sai_samplerate.h
  * @brief          : SAI Sample Rate Calculation from Register Settings
  * @author         : michael ruck + claude
  ******************************************************************************
  * @attention
  *
  * This file provides a function to calculate the actual SAI sample rate
  * based on hardware register settings for STM32G4xx microcontrollers.
  *
  ******************************************************************************
  */

#ifndef __MR_SAI_SAMPLERATE_H
#define __MR_SAI_SAMPLERATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"
#include "usbprintf.h"

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Berechnet die tatsächliche SAI Sample-Rate aus den Registereinstellungen
 * @param  hsai: Pointer auf SAI Handle
 * @retval Tatsächliche Sample-Rate in Hz (0.0f bei Fehler)
 */
float get_actual_SAI_sample_rate(SAI_HandleTypeDef *hsai)
{
    uint32_t sai_clock_freq = 0;
    float sample_rate = 0.0f;

    // Alle SAI Register auslesen
    uint32_t sai_cr1 = hsai->Instance->CR1;
#ifdef DEBUG
    uint32_t sai_cr2 = hsai->Instance->CR2;
#endif
    uint32_t sai_frcr = hsai->Instance->FRCR;
#ifdef DEBUG
    uint32_t sai_slotr = hsai->Instance->SLOTR;
#endif

    // RCC Register auslesen

    uint32_t rcc_cr = RCC->CR;
    uint32_t rcc_pllcfgr = RCC->PLLCFGR;
    uint32_t rcc_ccipr = RCC->CCIPR;

    // SAI CR1 Felder extrahieren
#ifdef DEBUG
    uint32_t mode = (sai_cr1 & SAI_xCR1_MODE_Msk) >> SAI_xCR1_MODE_Pos;
    uint32_t prtcfg = (sai_cr1 & SAI_xCR1_PRTCFG_Msk) >> SAI_xCR1_PRTCFG_Pos;
    uint32_t ds = (sai_cr1 & SAI_xCR1_DS_Msk) >> SAI_xCR1_DS_Pos;
#endif
    uint32_t mckdiv = (sai_cr1 & SAI_xCR1_MCKDIV_Msk) >> SAI_xCR1_MCKDIV_Pos;
    uint32_t nodiv = (sai_cr1 & SAI_xCR1_NODIV_Msk) >> SAI_xCR1_NODIV_Pos;
#ifdef DEBUG
    uint32_t dmaen = (sai_cr1 & SAI_xCR1_DMAEN_Msk) >> SAI_xCR1_DMAEN_Pos;
    uint32_t mcken = (sai_cr1 & SAI_xCR1_MCKEN_Msk) >> SAI_xCR1_MCKEN_Pos;
#endif

    // SAI FRCR Felder extrahieren
    uint32_t frl = ((sai_frcr & SAI_xFRCR_FRL_Msk) >> SAI_xFRCR_FRL_Pos) + 1;
#ifdef DEBUG
    uint32_t fsall = ((sai_frcr & SAI_xFRCR_FSALL_Msk) >> SAI_xFRCR_FSALL_Pos) + 1;
    uint32_t fsdef = (sai_frcr & SAI_xFRCR_FSDEF_Msk) >> SAI_xFRCR_FSDEF_Pos;
    uint32_t fspol = (sai_frcr & SAI_xFRCR_FSPOL_Msk) >> SAI_xFRCR_FSPOL_Pos;
    uint32_t fsoff = (sai_frcr & SAI_xFRCR_FSOFF_Msk) >> SAI_xFRCR_FSOFF_Pos;

    // SAI SLOTR Felder extrahieren
    uint32_t fboff = (sai_slotr & SAI_xSLOTR_FBOFF_Msk) >> SAI_xSLOTR_FBOFF_Pos;
    uint32_t slotsz = (sai_slotr & SAI_xSLOTR_SLOTSZ_Msk) >> SAI_xSLOTR_SLOTSZ_Pos;
    uint32_t nbslot = ((sai_slotr & SAI_xSLOTR_NBSLOT_Msk) >> SAI_xSLOTR_NBSLOT_Pos) + 1;
    uint32_t sloten = (sai_slotr & SAI_xSLOTR_SLOTEN_Msk) >> SAI_xSLOTR_SLOTEN_Pos;
#endif

    // SAI Clock Source ermitteln
    uint32_t sai1sel = (rcc_ccipr & RCC_CCIPR_SAI1SEL_Msk) >> RCC_CCIPR_SAI1SEL_Pos;

    switch(sai1sel)
    {
        case 0: // SYSCLK
            sai_clock_freq = HAL_RCC_GetSysClockFreq();
            break;

        case 1: // PLL "P" clock
            if(rcc_cr & RCC_CR_PLLRDY)
            {
                uint32_t pllsrc = (rcc_pllcfgr & RCC_PLLCFGR_PLLSRC_Msk) >> RCC_PLLCFGR_PLLSRC_Pos;
                uint32_t pllm = ((rcc_pllcfgr & RCC_PLLCFGR_PLLM_Msk) >> RCC_PLLCFGR_PLLM_Pos) + 1;
                uint32_t plln = (rcc_pllcfgr & RCC_PLLCFGR_PLLN_Msk) >> RCC_PLLCFGR_PLLN_Pos;
                uint32_t pllpdiv = (rcc_pllcfgr & RCC_PLLCFGR_PLLPDIV_Msk) >> RCC_PLLCFGR_PLLPDIV_Pos;
                uint32_t pllp;

                if(pllpdiv != 0)
                    pllp = pllpdiv;
                else
                    pllp = ((rcc_pllcfgr & RCC_PLLCFGR_PLLP_Msk) >> RCC_PLLCFGR_PLLP_Pos) ? 17 : 7;

                uint32_t pll_input = 0;
                if(pllsrc == 2) pll_input = 16000000; // HSI16
                else if(pllsrc == 3) pll_input = HSE_VALUE; // HSE

                if(pll_input > 0)
                    sai_clock_freq = (pll_input / pllm) * plln / pllp;
            }
            break;

        case 2: // External clock
            sai_clock_freq = 0;
            break;

        case 3: // HSI16
            sai_clock_freq = 16000000;
            break;
    }

#ifdef DEBUG
    usb_printf("\n=== SAI Sample Rate Debug Info ===\n");
    usb_printf("RCC Registers:\n");
    usb_printf("  CR        : 0x%08lX\n", rcc_cr);
    usb_printf("  PLLCFGR   : 0x%08lX\n", rcc_pllcfgr);
    usb_printf("  CCIPR     : 0x%08lX\n", rcc_ccipr);
    usb_printf("  SAI1SEL   : %lu\n", sai1sel);
    usb_printf("  SAI_CLK   : %lu Hz\n", sai_clock_freq);
    usb_printf("\nSAI Registers:\n");
    usb_printf("  CR1       : 0x%08lX\n", sai_cr1);
    usb_printf("  CR2       : 0x%08lX\n", sai_cr2);
    usb_printf("  FRCR      : 0x%08lX\n", sai_frcr);
    usb_printf("  SLOTR     : 0x%08lX\n", sai_slotr);
    usb_printf("\nSAI CR1 Fields:\n");
    usb_printf("  MODE      : %lu\n", mode);
    usb_printf("  PRTCFG    : %lu\n", prtcfg);
    usb_printf("  DS        : %lu\n", ds);
    usb_printf("  MCKDIV    : %lu\n", mckdiv);
    usb_printf("  NODIV     : %lu\n", nodiv);
    usb_printf("  DMAEN     : %lu\n", dmaen);
    usb_printf("  MCKEN     : %lu\n", mcken);
    usb_printf("\nSAI FRCR Fields:\n");
    usb_printf("  FRL       : %lu\n", frl);
    usb_printf("  FSALL     : %lu\n", fsall);
    usb_printf("  FSDEF     : %lu\n", fsdef);
    usb_printf("  FSPOL     : %lu\n", fspol);
    usb_printf("  FSOFF     : %lu\n", fsoff);
    usb_printf("\nSAI SLOTR Fields:\n");
    usb_printf("  FBOFF     : %lu\n", fboff);
    usb_printf("  SLOTSZ    : %lu\n", slotsz);
    usb_printf("  NBSLOT    : %lu\n", nbslot);
    usb_printf("  SLOTEN    : 0x%04lX\n", sloten);
#endif

    // Sample Rate Berechnung
    if(sai_clock_freq == 0)
    {
        sample_rate = 0.0f;
    }
    else if(nodiv == 0) // Master clock divider aktiviert
    {
        // Korrekte Formel: fs = SAI_CLK / MCKDIV / 256
        // MCKDIV wird direkt verwendet (nicht +1, nicht *2)
        if(mckdiv == 0)
        {
            // Wenn MCKDIV = 0, dann kein Teiler (oder Teiler = 1)
            sample_rate = (float)sai_clock_freq / 256.0f;
        }
        else
        {
            sample_rate = (float)sai_clock_freq / (float)mckdiv / 256.0f;
        }
    }
    else // NODIV = 1, kein Master clock divider
    {
        // FS = SAI_CK / FRL
        sample_rate = (float)sai_clock_freq / (float)frl;
    }

#ifdef DEBUG
    usb_printf("\nCalculation:\n");
    usb_printf("  Sample Rate: %.6f Hz\n", sample_rate);
    usb_printf("================================\n\n");
#endif

    return sample_rate;
}

#ifdef __cplusplus
}
#endif

#endif /* __MR_SAI_SAMPLERATE_H */
