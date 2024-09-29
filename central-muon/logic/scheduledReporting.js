import Particle from 'particle:core';

const reportableAqs = ['High', 'Danger'];

function createPayload(data) {
  return JSON.stringify(data);
}

function shouldReport(data) {
  if (data.aqs && reportableAqs.includes(data.aqs)) return true;
  if (data.pir === 'Detected') return true;
  if (data.magnet === 'Open') return true;
  if (data.sound === 'Above') return true;
}

export default function job({ functionInfo, trigger, scheduled }) {
  const deviceLedger = Particle.ledger("security-status", { deviceId: 'muon_0' });
  const data = deviceLedger.get().data;
  
  if (data && shouldReport(data)) {
    Particle.publish('SEC_ALERT', createPayload(data), { productId: 'muon' });
  }
}