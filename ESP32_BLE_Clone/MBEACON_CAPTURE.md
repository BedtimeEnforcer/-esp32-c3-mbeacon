# MBeacon 广播样本解析

用户提供的地址：`C3:00:00:1B:95:0F`；名称：`MBeacon`；扫描时 RSSI：`-87 dBm`。RSSI 是接收端的测量值，不在广播数据内。

原始十六进制共 **61 字节**，按 AD 字段长度解析为：

| 位置 | AD 类型 | 内容 |
|---|---|---|
| 0–2 | `0x01` Flags | `06` |
| 3–29 | `0xFF` Manufacturer Data | `4C000215FDA50693A4E24FB1AFCFC6EB0764782527D948E3C5` |
| 30–32 | `0x0A` TX Power | `00`，字段声明 0 dBm |
| 33–36 | `0x03` Complete 16-bit Service UUID | `A5FD`，按 BLE 小端序为 `0xFDA5` |
| 37–51 | `0x16` 16-bit Service Data | `A5FD6427D948E3C300001B950F` |
| 52–60 | `0x09` Complete Local Name | `MBeacon` |

**如果这是传统广播与 Scan Response 的合并显示**，两包各最多 31 字节，按完整 AD 字段切分时唯一可行的分界是 **ADV 前 30 字节，Scan Response 后 31 字节**。这份导出没有直接标注包边界，仍需设备扫描日志确认；若目标使用扩展广播，61 字节也可能来自单个扩展广告数据集。

```text
ADV (30 B): 0201061AFF4C000215FDA50693A4E24FB1AFCFC6EB0764782527D948E3C5
RSP (31 B): 020A000303A5FD0E16A5FD6427D948E3C300001B950F08094D426561636F6E
```

Manufacturer Data 符合常见 iBeacon 布局：Apple Company ID `0x004C`、前缀 `0215`、UUID `FDA50693-A4E2-4FB1-AFCF-C6EB07647825`、Major `0x27D9`（10201）、Minor `0x48E3`（18659）、参考功率 `0xC5`（-59 dBm）。Scan Response 的 Service Data 以 UUID `0xFDA5` 开头，后续 `64 27D9 48E3 C3 00 00 1B 95 0F`；末尾六字节与报告的 MAC 一致。`0x64` 的含义及哪些字节会隔几天变化，**单一样本无法确定**。

本记录没有标出 BLE **地址类型**（public/random）或 PDU 广播类型。`C3` 若属于 random 地址，最高两位 `11` 符合随机静态地址格式；仍需在设备上读取地址类型后才能确认同地址广播路径。现场扫描的串口输出会提供地址类型和广播类型。

数据类型依据：[Bluetooth SIG Assigned Numbers](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/index-en.html)、[Bluetooth SIG 的 iBeacon 格式说明](https://www.bluetooth.com/blog/advertising-works-part-1/)、[Espressif 的 ADV/Scan Response 说明](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/ble/get-started/ble-device-discovery.html)。

