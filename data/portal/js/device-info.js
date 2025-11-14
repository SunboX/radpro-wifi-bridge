;(function () {
    const hasDeviceInfo = () => document.getElementById('deviceId') !== null
    const setField = (id, val, unit = '') => {
        const el = document.getElementById(id)
        if (!el) return
        if (val === null || val === undefined || val === '') {
            el.textContent = '—'
            return
        }
        el.textContent = unit ? `${val} ${unit}`.trim() : val
    }

    let lastMeasurementSeconds = null

    const updateMeasurementAgeDisplay = () => {
        if (!hasDeviceInfo()) return
        if (lastMeasurementSeconds === null) {
            setField('measurementAge', '—')
            return
        }
        const translated =
            typeof portalTranslate === 'function' ? portalTranslate('T_MEASUREMENT_AGE', { seconds: lastMeasurementSeconds }) : null
        if (!translated || translated === 'T_MEASUREMENT_AGE') {
            setField('measurementAge', '—')
        } else {
            setField('measurementAge', translated)
        }
    }

    async function refreshDeviceInfo() {
        if (!hasDeviceInfo()) return
        try {
            const resp = await fetch('/device.json', { cache: 'no-store' })
            if (!resp.ok) throw new Error('bad status')
            const data = await resp.json()
            setField('manufacturer', data.manufacturer)
            setField('model', data.model)
            setField('firmware', data.firmware)
            setField('deviceId', data.deviceId)
            setField('locale', data.locale)
            const powerLabel =
                data.devicePower === '1'
                    ? portalTranslate('T_POWER_ON')
                    : data.devicePower === '0'
                    ? portalTranslate('T_POWER_OFF')
                    : data.devicePower
            setField('devicePower', powerLabel)
            setField('batteryVoltage', data.batteryVoltage)
            setField('batteryPercent', data.batteryPercent ? `${data.batteryPercent} %` : data.batteryPercent)
            setField('tubeRate', data.tubeRate)
            setField('tubeDoseRate', data.tubeDoseRate)
            setField('tubePulseCount', data.tubePulseCount)
            if (data.measurementAgeMs !== null && data.measurementAgeMs !== undefined) {
                lastMeasurementSeconds = (data.measurementAgeMs / 1000).toFixed(1)
            } else {
                lastMeasurementSeconds = null
            }
            updateMeasurementAgeDisplay()
        } catch (err) {
            console.warn('Device info refresh failed', err)
        }
    }

    document.addEventListener('portal-locale-ready', updateMeasurementAgeDisplay)
    document.addEventListener('DOMContentLoaded', () => {
        if (!hasDeviceInfo()) return
        refreshDeviceInfo()
        setInterval(refreshDeviceInfo, 10000)
    })
})()
