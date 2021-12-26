const Influx = require('influx');
const influx = new Influx.InfluxDB({
 host: '10.40.58.52',
 database: 'nakey',
 username:'admin',
 password:'gearman1',
 schema: [
   {
     measurement: 'response_times',
     fields: {
       path: Influx.FieldType.STRING,
       duration: Influx.FieldType.INTEGER
     },
     tags: [
       'host'
     ]
   }
 ]
})

influx.writePoints([
      {
        measurement: 'tide',
        tags: {
          unit: 'm',
          location: 'x',
        },
        fields: { height:10 },
        timestamp: 0,
      }
    ], {
      database: 'nakey',
      precision: 's',
    })
    .catch(err => {
      console.error(`Error saving data to InfluxDB! ${err.stack}`)
    });
