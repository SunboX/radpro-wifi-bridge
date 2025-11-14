;(function () {
    const hasBridgeInfo = () => document.getElementById('bridgeFirmware') !== null

    const setField = (id, val) => {
        const el = document.getElementById(id)
        if (!el) return
        if (val === null || val === undefined || val === '') {
            el.textContent = '—'
            return
        }
        el.textContent = val
    }

    async function refreshBridgeInfo() {
        if (!hasBridgeInfo()) return
        try {
            const resp = await fetch('/bridge.json', { cache: 'no-store' })
            if (!resp.ok) throw new Error('bad status')
            const data = await resp.json()
            setField('chipRev', data.chipRevision)
            setField('sdkVersion', data.sdkVersion)
            setField('bridgeFirmware', data.bridgeFirmware)
            setField('heapFree', data.heapFree ? `${(data.heapFree / 1024).toFixed(1)} kB` : null)
            setField('heapMax', data.heapMax ? `${(data.heapMax / 1024).toFixed(1)} kB` : null)
            setField('wifiMode', data.wifiMode)
            setField('ipAddress', data.ipAddress)
            setField('wifiRSSI', data.wifiRSSI)
            setField('macAddress', data.macAddress)
        } catch (err) {
            console.warn('Bridge info refresh failed', err)
        }
    }

    document.addEventListener('DOMContentLoaded', () => {
        if (!hasBridgeInfo()) return
        refreshBridgeInfo()
        setInterval(refreshBridgeInfo, 4000)
    })
})()
