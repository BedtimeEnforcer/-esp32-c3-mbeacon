# MBeacon ESP32-C3 学习与广播工具

目标板：合宙 LuatOS ESP32C3-CORE，板载 4 MB Flash、BOOT 键 GPIO9、D4 LED GPIO12。**无需焊接**。有 CH343 USB 转串口版和原生 USB 直连版，分别编译 `luatos_classic` 和 `luatos_usb` 环境。Flash 使用 DIO 模式，以便使用板载 D4 LED。

首次启动从用户提供的 61 字节 MBeacon 样本预置广播：固定地址 `C3:00:00:1B:95:0F`；传统广播数据 30 字节、扫描响应 31 字节。该切分、随机静态地址类型和可扫描非连接广播类型是从单份 nRF Connect 报告推断的，需要实机核对。广播间隔暂设 250 ms。

## 操作

- 上电默认广播。D4 慢闪（每 700 ms 翻转）。
- **正常启动后**短按 BOOT 进入扫描，D4 快闪（每 150 ms 翻转）。扫描持续进行，不设 30 秒超时；未捕获完整目标数据时保持快闪。
- 捕获到目标 MAC 的完整 MBeacon ADV + Scan Response 后，若数据变化则写入 NVS 并读回核对；若与已保存数据相同则直接确认。广播成功启动后 D4 常亮 3 秒、熄灭 0.2 秒，然后进入模拟模式慢闪。字节未变化也有同样的成功灯号。
- 扫描中再次短按 BOOT 可手动取消并恢复广播旧数据，但不会显示成功灯号。按住 BOOT 再复位会进入下载模式，不能用于普通切换。
- USB 串口 115200、换行发送命令：`SCAN`、`START`、`STOP`、`INFO`、`LOG`、`SAMPLE`、`CLEAR`、`TIME <Unix秒>`。无须按键也能操作。
- `SAMPLE` 恢复出厂样本并立即广播；`CLEAR` 删除 NVS 快照和 LittleFS 日志并停止，重启也不会自动恢复样本；之后可 `SCAN` 重新学习。
- 扫描到目标后打印 MAC、RSSI、广播类型、UUID、manufacturer data、service data、完整 ADV 和扫描响应。有效数据变化时打印 `OLD`、`NEW`、`UPDATED` 并存 NVS；无变化不写闪存。第一次真实扫描也会替换推断的地址和广播类型。只有 ADV 中的 iBeacon 厂商字段和扫描响应中的服务数据、名称都符合这份 MBeacon 样本格式，才算一次成功学习，避免把缺少扫描响应的半包覆盖原数据。
- `LOG` 输出设备内部的 `/scan_log.txt`；容量达到 64 KiB 后从头记录。若未发 `TIME`，绝对时间为 0，只记录启动后毫秒数。

## 构建

PlatformIO + Arduino Core 2.0.17 + NimBLE-Arduino 2.3.7；NimBLE 源码包含在 `lib/`，无需再次下载该库。PlatformIO 和板级包仍需安装。执行：

```powershell
pio run -e luatos_classic
pio run -e luatos_usb
```

`include/config.h` 中可修改目标地址、广播间隔和引脚。项目结构：`src/main.cpp` 模式与命令、`ble_scanner.cpp` 扫描、`ble_advertiser.cpp` 广播、`storage.cpp` NVS 与日志、`button.cpp` BOOT 消抖、`sample.cpp` 预置数据。

## 边界

- 能重播传统 ADV/Scan Response 的 AD 字节，不能保证复制射频细节、实际 RSSI、精确广播时序或目标 GATT 服务。
- 单份报告没有标出地址类型与空中 PDU 类型；预置样本对这两项使用推断。后续扫描会保存实测报告。
- 相同 MAC 的公共地址或私有随机地址未实现同地址重播；扫描到这类地址时会明确提示、不覆盖已有数据，也不会用备用地址假装成功。
- 广播启动失败会输出失败阶段，扫描模式每 5 秒才尝试一次，避免串口刷屏；失败时不会显示 3 秒成功灯号。
- 支持传统 ADV_IND、ADV_SCAN_IND 和 ADV_NONCONN_IND 的类型映射；定向广播与扩展广播暂不支持。PDU 类型和地址类型仍需实机扫描确认。
- 若手机 App 要连接、认证、校验滚动码、时间戳或加密数据，广播重播不保证有效。
- 本工程已编译验证，尚无开发板，未做实机烧录或手机 App 验收。

详情见 `MBEACON_CAPTURE.md`、`TECHNICAL_ANALYSIS.md` 和发行包中的图文手册。
