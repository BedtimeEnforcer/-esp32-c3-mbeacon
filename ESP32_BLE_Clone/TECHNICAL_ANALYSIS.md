# 一些技术笔记

写固件时查过、试过的东西记在这里，免得以后忘了为什么这么写。设计依据是 ESP32-C3 和 NimBLE-Arduino 的公开接口文档。

## 扫描和学习

- 没有按 MAC 的硬件过滤，就是全量扫描回来在代码里按地址比对。目标如果用随机地址、会定期换，固定 MAC 就盯不住。
- 主动扫描，周期 100 ms、窗口 90 ms，一直扫到成功为止，没有 30 秒超时一说。
- 判定学成功的条件比"看见目标 MAC"严：要收到一整套对得上 MBeacon 格式的 ADV + Scan Response（iBeacon 厂商字段、0xFDA5 服务数据、MBeacon 名称都在）。学到后写 NVS 并读回校验，跟已保存的快照相同就不重复写。
- 扫描输出里 Raw Advertisement Packet 和 Raw Scan Response 是分开打的，别把扫描响应跟 ADV 当成同一个空中包。

## 广播重播

- manufacturer / service data 都能自定义构造，本工程图省事，直接保存收到的 AD 原始字节再原样重播。
- 广播地址只能走 NimBLE 的 setOwnAddr 设随机地址；本工程只对合法的随机静态目标地址用同地址重播。想用任意公共地址得在初始化前设自定义 base MAC（ESP-IDF 支持），本版没做这条路。
- 所以重播不是完整克隆：AD 字节能对上，但地址类型、PDU 类型、广播间隔、功率、能不能被连接，这些都没法保证跟原设备一模一样。

## 存储

NVS 里只存一个 snapshot blob：一份里放齐 deviceMac、rawData（ADV 和 Scan Response 按顺序合并，两个包的长度单独记）、地址类型、广播类型、更新时间。一次写一份，是怕掉电时多个 key 写一半互相不一致。updatedEpoch 只有在串口发过 TIME 之后才是真实时间，板子本身没有后备时钟。

## 板子（合宙 ESP32C3-CORE）

- 官方资料：4 MB Flash、BOOT 是 GPIO9、D4 是 GPIO12、D5 是 GPIO13。本工程拿 BOOT 做模式切换、D4 做状态灯，不用焊东西。注意 BOOT 要在正常启动之后再按，按住 BOOT 复位是进下载模式。
- GPIO12 能当 LED 用，前提是这块板是外置 Flash、DIO 模式，所以构建和合并镜像都固定 DIO。拿到内置 Flash 变体的别直接套这份配置。
- 这块板有 CH343 串口的经典版和原生 USB 直连版，所以固件出两份：luatos_classic（UART0 的 Serial）和 luatos_usb（USB CDC 的 Serial），BLE 逻辑一样。

## 还没在实物上验证的

编译通过只说明 API 和语法没问题。下面这些要拿真板子测才算数：目标的地址类型和广播类型、ADV/Scan Response 的分界对不对、目标广播会不会变、手机 App 是否还依赖连接或配对。目前只有一个 MBeacon 抓包样本（见 MBEACON_CAPTURE.md），单一样本说明不了 App 就一定认这个重播。
