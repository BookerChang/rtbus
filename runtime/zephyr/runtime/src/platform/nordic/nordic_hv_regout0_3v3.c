#include <zephyr/init.h>
#include <hal/nrf_power.h>

#if defined(CONFIG_SOC_NRF52840)

static int nordic_hv_regout0_3v3_init(void)
{
    uint32_t regout0_vout = NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk;

    if ((nrf_power_mainregstatus_get(NRF_POWER) != NRF_POWER_MAINREGSTATUS_HIGH) ||
        (regout0_vout == (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos))) {
        return 0;
    }

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
        __NOP();
    }

    NRF_UICR->REGOUT0 =
        (NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
        (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
        __NOP();
    }

    NVIC_SystemReset();

    return 0;
}

SYS_INIT(nordic_hv_regout0_3v3_init, PRE_KERNEL_1, 0);
#endif