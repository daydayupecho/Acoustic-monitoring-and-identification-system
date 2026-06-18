Base: user's runnable Tx_LowPower_1 project.
Added minimal visible heartbeat debug only:
- USART1 PA9/PA10, 115200 8N1
- Boot prints in main()
- Main ThreadX thread prints heartbeat periodically and toggles LED1
- ThreadX low-power hooks are left as no-op to keep UART alive
No USC/audio functionality added.
