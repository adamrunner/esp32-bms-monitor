### LED Color & Pattern Scheme

#### **Color Mapping by Priority**
- 🔴 **RED** - Critical errors/safety issues (highest priority)
- 🟠 **ORANGE** - Warnings/degraded operation 
- 🟡 **YELLOW** - Boot sequence/initialization
- 🟢 **GREEN** - Normal operation/success
- 🔵 **BLUE** - Network/connectivity status
- 🟣 **PURPLE** - OTA/maintenance operations
- ⚪ **WHITE** - System idle/standby

#### **Pattern Types**
- **Solid** - Stable state
- **Slow pulse** (1Hz) - Transitional/working state  
- **Fast blink** (5Hz) - Alert/attention needed
- **Breathe** - Standby/waiting state
- **Rainbow cycle** - Boot sequence
- **OFF** - System shutdown/error

#### **Specific Status Indicators**

**🚨 CRITICAL (RED)**
- Fast blink: BMS communication failure
- Solid: Cell overvoltage/undervoltage critical
- Pulse: Temperature critical limits exceeded
- Double blink: MOSFET failure/protection activated

**⚠️ WARNINGS (ORANGE)**  
- Slow pulse: Low SOC warning (<20%)
- Solid: WiFi connection lost but retrying
- Fast blink: High cell voltage delta (>0.2V)
- Breathe: Temperature warning thresholds

**🔧 INITIALIZATION (YELLOW)**
- Rainbow cycle: System boot sequence
- Pulse: WiFi connecting
- Solid: BMS initialization in progress
- Breathe: Waiting for time sync

**✅ NORMAL OPERATION (GREEN)**
- Solid: All systems normal, BMS healthy
- Slow pulse: Charging active
- Breathe: Discharging active

**🌐 NETWORK (BLUE)**  
- Solid: WiFi connected, strong signal (RSSI > -60dBm)
- Slow pulse: WiFi connected, weak signal (RSSI < -70dBm)  
- Fast blink: MQTT connection issues
- Breathe: Network services degraded

**🔄 MAINTENANCE (PURPLE)**
- Pulse: OTA check in progress
- Solid: OTA download/install active
- Fast blink: OTA failed/rollback needed
