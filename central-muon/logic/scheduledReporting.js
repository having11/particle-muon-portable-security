import Particle from 'particle:core';

const reportableAqs = ['High', 'Danger'];

function shouldReport(data) {
  if (data.aqs && reportableAqs.includes(data.aqs)) return true;
  if (data.pir === 'Detected') return true;
  if (data.magnet === 'Open') return true;
  if (data.sound === 'Above') return true;
  
  return false;
}

export default function job({ functionInfo, trigger, scheduled }) {
  const deviceLedger = Particle.ledger("security-status", { deviceId: 'DEVICE_ID' });
  const data = deviceLedger.get().data;
  console.log('found data', JSON.stringify(data));
  
  if (data && shouldReport(data)) {
    console.log('publishing alert!');
    Particle.publish('SEC_ALERT', 'Security alert', { productId: 12345 });
  }
}