# Web roll call / 网页点名

After connecting to C01, subscribe to the existing BuddyGuard Event characteristic and complete `SYNC`. Connect the **Roll call / 点名** button to the Control characteristic below.

- App service `39C2AE25-C509-4902-BAD6-FA7D52CBBB86`
- Control `7F3F8D94-CB46-46A7-AC11-91D75BACF707`, Write With Response
- Event `8ED898B1-50A1-4DAD-8779-2BFA73ED78B9`, Indicate

```js
async function rollCall(controlCharacteristic) {
  const frame = new Uint8Array(16);
  frame.set([1, 3, 0, 0]);
  const [requestId] = crypto.getRandomValues(new Uint32Array(1));
  new DataView(frame.buffer).setUint32(4, requestId || 1, true);
  await controlCharacteristic.writeValueWithResponse(frame);
}
```

C01 snapshots the enrolled roster and requests each tag's location over LoRa. Each tag takes one snapshot of its recent valid GPS fix. Radio retries reuse that snapshot. C01 forwards each matched fix as one existing `TAG_POSITION` indication (event kind 5). `ROLL_CALL` retries at the BLE layer should reuse the same Control frame and request ID for the same click.

点名按钮写入 16 字节 Control 帧，前 4 字节是 `01 03 00 00`。偏移 4 写入非零的小端请求号，最后 8 字节保持 0。每次点击生成新请求号。C01 逐个向已登记标签发定位请求，每个标签只取一次当前有效定位，LoRa 重试沿用同一份结果。

The map reads the tag ID at offset 2 and the fix age in seconds at offset 3. Latitude and longitude are signed little-endian `int32` at offsets 8 and 12, divided by `10^7`. A successful Control write means C01 queued the round. A tag with no fresh fix leaves its existing map marker unchanged. Mark older fixes with their age.

地图继续用已有的 `TAG_POSITION` 解析逻辑。偏移 2 是标签编号，偏移 3 是定位年龄（秒），偏移 8 和 12 是小端有符号经纬度，除以 `10^7`。没有新定位时保留上次的位置并显示时间。固件可用 C01 串口的 `bg locate` 触发同一条 LoRa 路径。
