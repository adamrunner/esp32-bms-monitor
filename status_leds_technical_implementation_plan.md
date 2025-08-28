### Technical Implementation Plan

#### **Component Architecture**
```
components/status_led/
├── include/status_led.h          # Public API  
├── CMakeLists.txt               # Component config
├── status_led.cpp               # Core LED control
├── status_evaluator.cpp         # Status priority logic
└── led_patterns.cpp             # Pattern generators
```

#### **Key Features**
1. **Status Priority System** - Higher priority statuses override lower ones
2. **Status Aggregation** - Collect status from all system components  
3. **Pattern Engine** - Smooth animations and transitions
4. **Thread-Safe Updates** - Multiple components can update status safely
5. **Configuration Support** - Enable/disable via SPIFFS config
6. **Memory Efficient** - Minimal RAM footprint using ESP32 RMT peripheral

#### **Integration Points**
- **main.cpp:46-63** - WiFi status integration
- **main.cpp:167** - BMS communication status  
- **main.cpp:289** - BMS data validation
- **main.cpp:294-308** - WiFi periodic status check
- **ota_status_logger.cpp** - OTA progress indication
- **log_manager.cpp** - Logging pipeline health

#### **Status Update Frequency**
- **Critical safety checks**: Every BMS read cycle (10s)
- **Network status**: Every 10 BMS readings (~100s) 
- **System health**: Every minute
- **Boot sequence**: Real-time during initialization

#### **Configuration Options**
```
/spiffs/status_led_config.txt:
enabled=true
gpio_pin=8
brightness=64          # 0-255
boot_animation=true
critical_override=true # Always show critical alerts
```

#### **Memory & Performance**
- **RAM usage**: ~2KB for pattern buffers and state
- **CPU overhead**: <1% using ESP32 RMT hardware acceleration
- **Boot time impact**: <100ms additional initialization
- **Power consumption**: ~20mA @ 25% brightness

This implementation provides comprehensive system visibility while maintaining the existing codebase architecture and following ESP-IDF conventions.