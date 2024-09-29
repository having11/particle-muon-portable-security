# Edge Impulse ADXL345 Library for Particle devices


## Install
User Particle tools to install this library into your project. Either Workbench IDE or Particle CLI works.
Alternative option is to copy / paste following files into your project:
- `adxl345.cpp`
- `adxl345.h`
- `binary.h`

## Example
The included example shows how to retrieve data from the sensor and forward the data over serial.
When using the Edge Impulse CLI tools data can forwarded into your project directly. 

## Further info

### Running inference on a Particle device
Edge Impulse enabled running neural networks on Particle hardware. Follow this [example](https://docs.edgeimpulse.com/docs/run-inference/running-your-impulse-particle) on how to train and build an Edge AI model and deploy it to a Particle device.

### Use the data forwarder to upload sensor data directly into your project
Using the `data-forwarder` example, sensor data can be uploaded to an Edge Impulse studio project directly. This [docs page](https://docs.edgeimpulse.com/docs/tools/edge-impulse-cli/cli-data-forwarder) explains how to hook up hardware to Edge Impulse studio.


Original library created by Kenneth Lim <kennethlimcp@gmail.com>


