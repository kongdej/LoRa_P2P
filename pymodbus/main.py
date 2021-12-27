from pymodbus.client.sync import ModbusSerialClient as ModbusClient

modbus = ModbusClient(method='rtu', port='/dev/tty.wchusbserial14230', baudrate=9600, timeout=10)
connection = modbus.connect()
print ("Connection: ", connection)

#read_device_information
request = modbus.read_holding_registers(address = 0x0000,count = 0x02, unit=0x02)
#request = modbus.read_input_registers(address = 0x0000,count = 0x02, unit=0x01)
print(request.registers)

# change address
#res = modbus.write_register(address = 0x07D1 , value = 0x02, unit=0x01)





# t&h   2   :0x0020 = address, 0x0010 = baudrate
# wd    4   :0x07D0 = address, 0x07D1 = baudrate, ref:https://inhandgo.com/products/soil-temperature-and-moisture-sensor
# ws    6   :0x07D0 = address, 0x07D1 = baudrate, ref:https://inhandgo.com/products/soil-temperature-and-moisture-sensor
# soil  8   :0x07D0 = address, 0x07D1 = baudrate, ref:https://inhandgo.com/products/soil-temperature-and-moisture-sensor
