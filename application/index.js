var mqtt = require('mqtt');
const {InfluxDB} = require('@influxdata/influxdb-client')

// You can generate an API token from the "API Tokens Tab" in the UI
const token = 'jEgf4f07jQq0-9rNvkzvuJIaLjP0LQAXWVljjoL1CS06_f_gKsJxvmdaXKkqcj_mWnw78ZndDqrt4gr6aD5htA=='
const org = 'skhome'
const bucket = 'skbucket'
const clientdb = new InfluxDB({url: 'http://localhost:8086', token: token})

const {Point} = require('@influxdata/influxdb-client')

const MQTT_SERVER = "10.40.58.52";
const MQTT_PORT = "1883";
const MQTT_USER = "jd";
const MQTT_PASSWORD = "gearman1";
const TOPIC = "apps";

// Connect MQTT
var client = mqtt.connect({
    host: MQTT_SERVER,
    port: MQTT_PORT,
    username: MQTT_USER,
    password: MQTT_PASSWORD
});

client.on('connect', function () {
    console.log("MQTT Connect");
    // Subscribe any topic
    client.subscribe(TOPIC, function (err) {
      if (err) {
          console.log(err);
      }
    });
});

// Receive Message and print on terminal
client.on('message', function (topic, message) {
    // message topic
    if (topic == 'apps') {
      const obj = JSON.parse(message.toString())

      // Gateway No.2
      if ( obj.GW2 !== undefined ) {
        let payload = obj.GW2

        // header
        let gwId    = parseInt(payload.substring(2,4),16)
        let sender  = parseInt(payload.substring(4,6),16)
        let msgId   = parseInt(payload.substring(6,8),16)
        let msgLen  = parseInt(payload.substring(8,10),16)
        let rssi    = parseInt(payload.substring(10,12),16)

        console.log(message.toString(),'->',payload)
        console.log("gwId=",gwId, ", sender=",sender,", msgId=",msgId, ", msgLen=",msgLen, ", rssi=",rssi)

        // data payload node #1 -------------------------
        if( sender === 1 ) {
          let data    = parseInt(payload.substring(12,12 + msgLen),16)
          if (!isNaN(data)) {
            console.log("data =",data)
            const writeApi = clientdb.getWriteApi(org, bucket)
            writeApi.useDefaultTags({gateway: '2'})
            const point = new Point('Node-1')
                          .floatField('counter', data)

            writeApi.writePoint(point)
            writeApi.close().then(() => {
                    console.log('Save InfluxDB\n')
                }).catch(e => {
                    console.error(e)
                })
          }
        }

        // data payload node #2 -----------------------
        else if( sender === 2 ) {
            let t = parseInt(payload.substring(12,16),16)
            let h = parseInt(payload.substring(16,20),16)
            if (!isNaN(t) && !isNaN(h)) {
              console.log("t = ",t, ", h = ",h)
              const writeApi = clientdb.getWriteApi(org, bucket)
              writeApi.useDefaultTags({gateway: '2'})
              const point = new Point('Node-2')
                            .floatField('temperature', parseFloat(t)/10.0)
                            .floatField('humidity', parseFloat(h)/10.0)

              writeApi.writePoint(point)
              writeApi.close().then(() => {
                      console.log('Save InfluxDB\n')
                  }).catch(e => {
                      console.error(e)
                  })
            }
        }


      }
    }
});
