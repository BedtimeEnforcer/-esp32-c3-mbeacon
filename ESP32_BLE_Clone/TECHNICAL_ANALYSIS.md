# 技术分析与参考仓库核查

## 参考仓库

`home_wt/simulate-time-clock-release` 的本地完整克隆提交为 `c8becb6f73ab322441cef902ce686625a332df1c`。提交树只有 USB 串口驱动、烧录工具 ZIP、两个手机工具 APK、一个网址 TXT 和空 README；无 Arduino/ESP-IDF 源码。不能从它推断 BLE 初始化、MAC 设置、广播数据构造、payload 布局或是否支持 GATT。核查证据详见 `../reference/reference-notes.md`。

设计依据转为 ESP32-C3 与 NimBLE-Arduino 的公开接口，而不是该仓库的二进制固件。

## 能力与边界

| 问题 | 判定 |
|---|---|
| 扫描指定 MAC | 支持；实际是扫描全部广播后按地址过滤。随机地址变化时固定 MAC 过滤会失效。 |
| 自定义 manufacturer / service data | 支持，可构造 AD 字段；本工程直接保存和重播收到的 AD 字节。 |
| 自定义广播地址 | NimBLE-Arduino 的 `setOwnAddr` 设随机地址，`setOwnAddrType` 选随机地址。本工程仅对合法随机静态目标地址使用相同地址。ESP-IDF 也允许初始化前设置自定义 base MAC，并由它派生 Bluetooth 公共地址；本版未实现该启动流程。 |
| 完整复制目标广播 | 不能保证。可复现传统广播的 ADV/Scan Response AD 字节；地址类型、PDU 类型、间隔、功率及连接行为存在限制。 |

扫描模式输出 `MAC / RSSI / Advertisement Type / Service UUID / Manufacturer Data / Service Data / Raw Advertisement Packet / Raw Scan Response`。后一项单独输出，避免把扫描响应与 ADV 误当同一空中包。

日常学习采用主动扫描、100 ms 扫描周期中的 90 ms 窗口，持续到成功，不设置固定 30 秒超时。BLE 设备多时回调先过滤目标 MAC；只有收到符合当前 MBeacon 格式的完整 ADV 与 Scan Response（iBeacon 厂商字段、0xFDA5 服务数据和 MBeacon 名称）才认为学习成功。变化时写入 NVS 并读回校验；相同值沿用已保存快照。之后先启动广播，成功后 D4 常亮 3 秒、熄灭 0.2 秒，再慢闪。若未收到合格数据则继续快闪扫描。这样灯号表明收到完整可重播的数据，而不只是看见目标 MAC。

公共地址的硬件能力依据：[Espressif ESP32-C3 MAC 地址说明](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/misc_system_api.html)。自定义 base MAC 必须在初始化无线接口前设置，也会改变同芯片派生的其他接口地址；不能在扫描到目标后直接用当前 `setOwnAddr` 路径切换为任意公共地址。

NVS 使用一个 `snapshot` blob，内含 `deviceMac`、`rawData`、`rawLength`、`updatedEpoch`、`updatedUptimeMs`、地址类型、广播类型和完整 AD 字节。`rawData` 为 ADV 与 Scan Response 按顺序合并后的字节，两个包的长度单独保存；任一包变化就更新。单次写入一份快照，减少掉电时多个 key 之间出现不一致的机会。由于没有板载后备时钟，`updatedEpoch` 只有在串口设置 `TIME` 后才有绝对时间意义。

工程结构：`src/main.cpp` 处理模式与命令，`ble_scanner.*` 负责扫描入队，`ble_advertiser.*` 负责广播，`storage.*` 负责 NVS/LittleFS，`button.*` 负责消抖，`include/config.h` 放硬件与目标配置。

## 合宙 ESP32C3-CORE 板级适配

合宙官方板资料列出 4 MB Flash、BOOT/GPIO9、D4/GPIO12 和 D5/GPIO13。工程使用板载 BOOT 做模式切换、D4 做快慢闪，无需焊接。BOOT 在正常启动后才可按；按住 BOOT 再复位会进入下载模式。GPIO12 能作为 LED 是以该板使用外置 Flash、DIO 模式为前提；因此构建与合并镜像均固定 DIO。若拿到的是内置 Flash 变体，不能直接套用这份板级配置。

官方文档还区分 CH343 USB-UART 经典版与原生 USB 直连版。本工程分别构建 `luatos_classic`（UART0 `Serial`）和 `luatos_usb`（USB Serial/JTAG CDC `Serial`）两份固件。两者的 BLE 逻辑相同。参见[合宙板卡资料](https://wiki-zh.luatos.org/chips/esp32c3/board.html)。

## 现场验证门槛

软件编译通过只能确认 API 与语法。用户已提供一份 MBeacon 合并样本，详见 `MBEACON_CAPTURE.md`。以下仍需实物测量：目标地址类型与广播类型、ADV/Scan Response 分界的现场确认、广播变化规律、手机 App 是否依赖连接/GATT/配对。单一样本不能证明手机 App 会接受模拟结果。

