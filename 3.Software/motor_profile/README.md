# APP 246：内部电机参数

基于 `main` 的 APP 245，只集中电机规格和方向配置。默认值沿用 245；MC 通信、对象字典、EEPROM 布局、Bootloader、保护及原校准流程保持原样。

配置入口：`2.Firmware/ADM32F036_mc_app/user/motor_profile.c` 的 `motor_parameters` 结构体对象。每个参数都有中文注释；类型声明在 `motor_profile.h`。修改结构体初值后重新编译，没有新增通信对象或保存项。

| 参数 | 默认值 | 用途 |
| --- | --- | --- |
| `pole_pairs` | 10 | 电角度与校准扫描 |
| `gear_numerator` / `gear_denominator` | 12 / 1 | 减速比与位置、速度换算 |
| `main_teeth` / `aux_teeth` | 17 / 18 | 双编码器齿数，沿用相邻齿数算法 |
| `motor_parameters.output_reverse` | 0 | 关节正方向开关；1 时控制及上报的位置、速度、力矩方向同时翻转 |
| `current_base_da` | 280 | 厂商电流基值，单位 0.1 A |
| `current_limit_ma` | 35000 | 原 MIT 电流限幅，单位 mA |
| `calibration_current_ma` | 9667 | 原校准电流，换算后仍为 `Id=1000` |
| `torque_limit_mnm` | 30000 | 原力矩上限，单位 mN·m |
| `resistance_mohm` | 269 | 原厂商电阻参数，单位 mΩ |
| `ld_uh` / `lq_uh` | 270 / 326 | 原厂商 dq 电感参数，单位 µH |
| `initial_electrical_zero` | 1675 | 无有效记录时的初始电角度偏置 |

## 方向与校准

`motor_parameters.output_reverse = 0` 保持原来的关节正方向；若与期望相反，改成 `1` 后重新编译、烧录。控制指令、上报的位置和速度、力矩及有符号电流使用同一套方向。校准不会改写此参数，重新校准或重启后仍按这个编译配置生效。

电气相序和编码器方向继续由原校准流程自动探测并保存在旧记录中。关节正方向开关独立于这些电气校准数据，单独修改开关无需重做电气校准。

校准仍通过原命令 `0x2002 = 0xF1` 触发，完成后按原流程保存电气偏置、512 项 LUT、方向和校准状态。换电机、极对数或减速器后需重新校准；旧保存格式无法判断记录属于哪套电机规格。

实际快速路径为 `ZeroOfEPWMISR → encoder_loop → ADCOverInterrupt → MotorControlISR → CurrentLoop`；你的 `MC_servo_loop` 产生 `Iq/Id`，由原 `RunCaseRun` 送入电流环。方向、极对数及减速比接入这些实际使用的编码器、关节控制和反馈计算。

R/L 接入原 `PMSM_RS/PMSM_LD/PMSM_LQ → ParGetFromFunction` 参数换算。PI 继续使用 `MotorControlISR` 中原有的 `d/q Kp=420、Ki=250` 入口及后续厂商换算，没有新增目标带宽或 PI 自动整定逻辑。

你自己的绕组温度估算使用独立的 0.242/0.342 Ω 电阻基准和 0.043 Ω 寄生电阻，力矩标定也有独立的表。这些模型沿用原值，换型号后需单独验证与适配。

## 构建

CCS 按原方式编译即可。也可使用 Python 3 与 TI C2000 CGT 22.6.1.LTS：

```powershell
python -B 3.Software/motor_profile/build_app.py
```

脚本默认使用 `D:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS`，可用 `--compiler` 指定路径。电机参数直接修改 `motor_parameters` 的结构体初值。启动时校验配置并计算一次派生量，电流中断不做这些初始化换算；非法配置会停在外设及功率级初始化之前。

产物在 `2.Firmware/ADM32F036_mc_app/Debug/mc-245-compatible/`，其中 `ADM32F036_mc_app_released_fw.bin` 为 48 KiB APP OTA 包。构建会检查 COFF/BIN 内容一致性及发布包 CRC。

默认参数的离线控制回放、旧校准恢复、完整校准保存和旧版 OTA 仿真用于检查与 245 的兼容性。离线验证不代表已完成实板 OTA 或电机运行验证；一次性验证代码及结果不纳入版本管理。
