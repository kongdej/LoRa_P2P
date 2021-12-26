var mqtt = require('mqtt');
const Influx = require('influx')

const influx = new Influx.InfluxDB({
 host: '10.40.58.52',
 database: 'nakey',
 username:'admin',
 password:'gearman1'
})

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
      if ( obj.nakee !== undefined ) {
        let payload = obj.nakee

        // header
        let gwId    = parseInt(payload.substring(2,4),16)
        let sender  = parseInt(payload.substring(4,6),16)
        let msgId   = parseInt(payload.substring(6,8),16)
        let msgLen  = parseInt(payload.substring(8,10),16)
        let rssi    = parseInt(payload.substring(10,12),16)

        console.log(message.toString(),'->',payload)
        console.log("gwId=",gwId, ", sender=",sender,", msgId=",msgId, ", msgLen=",msgLen, ", rssi=",rssi)

        // data payload node #1 -------------------------
        if( gwId == 2 && sender === 1 ) {
          let data    = parseInt(payload.substring(12,12 + msgLen),16)
          if (!isNaN(data)) {
            console.log("data =",data)
          }
        }

        // data payload node #2 -----------------------
        else if( gwId == 2 && sender === 2 ) {
            let t = parseInt(payload.substring(12,16),16)
            let h = parseInt(payload.substring(16,20),16)
            if (!isNaN(t) && !isNaN(h)) {
              console.log("t = ",t, ", h = ",h)

              influx.writePoints([
                    {
                      measurement: 'n_'+gwId.toString()+'_'+sender.toString(),
                      tags: {
                        gid: gwId,
                      },
                      fields: {
                        temperature: parseFloat(t)/10.0,
                        humidity: parseFloat(h)/10.0
                      },
                    }
                  ], {
                    database: 'nakey',
                    precision: 's',
                  })
                  .catch(err => {
                    console.error(`Error saving data to InfluxDB! ${err.stack}`)
                  });
            }
        }


      }
    }
});
