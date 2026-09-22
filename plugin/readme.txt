注意：
1.结构体变量名称必须和动态库同名；
2.没有DeviceID的设备，DeviceID号由网关网卡MAC(12bytes)+设备类型(2bytes)+设备编号组成(主号(1byte)+次号(1byte)),DeviceID作为设备的主键（primaryItem）；
3.物模型设备由输出设备体现，必须有ProductID，非物模型设备必须删除ProductID，输出设备的外键=输入设备的主键，输入设备外键（ForeignItem）可以忽略，（例：KNX新风面板控制modbus新风）；
4.厂家型号
	SDK    <--> 思德克智能科技(深圳)有限公司
	Daikin <--> 大金工业株式会社
	TuoLi  <--> 拓力？？？
