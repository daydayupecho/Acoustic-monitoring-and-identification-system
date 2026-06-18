First migration patch on top of Tx_LowPower_1_heartbea.zip
Adds only AudioAcq + ADC1 + LPTIM1 + GPDMA1_Channel4. No USC.
Effective parameters aligned to TrustZoneDisabled_12_5_3 runtime acquisition path:
- LPTIM-triggered ADC
- 16 kHz trigger from HSI16 (ARR=999, Pulse=499)
- HSI trim = 14
- ADC1 CH7 on PA2
- ADC kernel clock = MSIK 2 MHz
- PB2 export OFF
- No AudioWindow/USC stage (RAW_DMA_ONLY)
After replacing: Refresh project, delete Debug, Clean, Build.
