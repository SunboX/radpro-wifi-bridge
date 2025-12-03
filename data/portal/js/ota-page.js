;(function () {
    const POLL_INTERVAL_MS = 4000
    const UPLOAD_CHUNK_SIZE = 16384

    const els = {
        currentVersion: document.getElementById('otaCurrentVersion'),
        latestVersion: document.getElementById('otaLatestVersion'),
        latestStatus: document.getElementById('otaLatestStatus'),
        statusText: document.getElementById('otaStatusText'),
        progressFill: document.getElementById('otaProgressFill'),
        progressNumbers: document.getElementById('otaProgressNumbers'),
        fetchBtn: document.getElementById('otaFetchBtn'),
        cancelBtn: document.getElementById('otaCancelBtn'),
        fileInput: document.getElementById('otaFileInput'),
        uploadBtn: document.getElementById('otaUploadBtn'),
        uploadMessage: document.getElementById('otaUploadMessage')
    }

    let pollTimer = null
    let manualBusy = false

    const safeText = (value, fallbackKey) => {
        if (typeof value === 'string' && value.length) return value
        if (fallbackKey) return portalTranslate(fallbackKey)
        return ''
    }

    const formatBytes = (bytes) => {
        if (!bytes) return '0 B'
        const units = ['B', 'KB', 'MB', 'GB']
        let unit = 0
        let value = bytes
        while (value >= 1024 && unit < units.length - 1) {
            value /= 1024
            unit++
        }
        const fixed = value >= 10 || unit === 0 ? value.toFixed(0) : value.toFixed(1)
        return `${fixed} ${units[unit]}`
    }

    const chunkToBase64 = (chunk) => {
        let binary = ''
        const max = 0x8000
        for (let i = 0; i < chunk.length; i += max) {
            const sub = chunk.subarray(i, i + max)
            binary += String.fromCharCode.apply(null, sub)
        }
        return btoa(binary)
    }

    const sendJson = async (url, options = {}) => {
        const response = await fetch(url, Object.assign({ headers: { 'Content-Type': 'application/json' } }, options))
        let data = null
        try {
            data = await response.json()
        } catch (err) {
            data = null
        }
        if (!response.ok) {
            const message = (data && data.error) || response.statusText || 'Request failed'
            throw new Error(message)
        }
        return data || {}
    }

    const sendPlain = async (url, body) => {
        const response = await fetch(url, {
            method: 'POST',
            headers: { 'Content-Type': 'text/plain' },
            body
        })
        let data = null
        try {
            data = await response.json()
        } catch (err) {
            data = null
        }
        if (!response.ok) {
            const message = (data && data.error) || response.statusText || 'Request failed'
            throw new Error(message)
        }
        return data || {}
    }

    const schedulePoll = () => {
        clearTimeout(pollTimer)
        pollTimer = setTimeout(fetchStatus, POLL_INTERVAL_MS)
    }

    const setStatusLine = (textKeyOrValue, vars) => {
        if (!els.statusText) return
        const text = vars ? portalTranslate(textKeyOrValue, vars) : textKeyOrValue
        els.statusText.textContent = text
    }

    const updateProgress = (written, total) => {
        if (!els.progressFill || !els.progressNumbers) return
        if (!total) {
            els.progressFill.style.width = '0%'
            els.progressNumbers.textContent = ''
            return
        }
        const pct = Math.min(100, Math.round((written / total) * 100))
        els.progressFill.style.width = `${pct}%`
        els.progressNumbers.textContent = `${formatBytes(written)} / ${formatBytes(total)}`
    }

    const setLatestStatus = (message, isError) => {
        if (!els.latestStatus) return
        els.latestStatus.textContent = message || ''
        if (isError) {
            els.latestStatus.classList.add('error')
        } else {
            els.latestStatus.classList.remove('error')
        }
    }

    const setUploadMessage = (message, isError) => {
        if (!els.uploadMessage) return
        els.uploadMessage.textContent = message || ''
        els.uploadMessage.classList.toggle('error', !!isError)
    }

    const updateStatusView = (payload) => {
        const ota = payload.ota || {}
        const busy = !!(ota.busy || ota.taskActive)

        if (els.currentVersion) {
            const current = payload.currentVersion || '—'
            els.currentVersion.textContent = current
        }

        if (els.latestVersion) {
            const latest = payload.latestVersion || '—'
            els.latestVersion.textContent = latest
        }

        if (payload.latestError) {
            setLatestStatus(payload.latestError, true)
        } else {
            setLatestStatus('', false)
        }

        if (els.fetchBtn) els.fetchBtn.disabled = busy
        if (els.cancelBtn) els.cancelBtn.disabled = !busy
        if (els.uploadBtn) els.uploadBtn.disabled = busy || manualBusy
        if (els.fileInput) els.fileInput.disabled = busy || manualBusy

        let statusText = ota.message || ''
        if (!statusText.length) {
            statusText = busy ? portalTranslate('T_REMOTE_STATUS_WORKING') : portalTranslate('T_REMOTE_STATUS_IDLE')
        }
        if (ota.needsReboot) {
            statusText = portalTranslate('T_REMOTE_STATUS_SUCCESS')
        } else if (ota.lastError && !busy) {
            statusText = ota.lastError
        }
        setStatusLine(statusText)

        updateProgress(ota.bytesWritten || 0, ota.bytesTotal || 0)
    }

    const fetchStatus = async () => {
        try {
            const response = await fetch('/ota/status')
            if (!response.ok) throw new Error(`HTTP ${response.status}`)
            const data = await response.json()
            updateStatusView(data)
        } catch (err) {
            setLatestStatus(err.message, true)
        } finally {
            schedulePoll()
        }
    }

    const triggerRemoteUpdate = async () => {
        if (!els.fetchBtn || els.fetchBtn.disabled) return
        try {
            els.fetchBtn.disabled = true
            setStatusLine(portalTranslate('T_REMOTE_STATUS_WORKING'))
            await sendJson('/ota/fetch', { method: 'POST' })
        } catch (err) {
            setStatusLine(err.message)
        } finally {
            els.fetchBtn.disabled = false
        }
    }

    const cancelRemoteUpdate = async () => {
        if (!els.cancelBtn || els.cancelBtn.disabled) return
        try {
            await sendJson('/ota/cancel', { method: 'POST' })
        } catch (err) {
            setStatusLine(err.message)
        }
    }

    const ensureReadyForUpload = () => {
        if (!els.fileInput || !els.fileInput.files.length) {
            setUploadMessage(portalTranslate('T_MANUAL_NO_FILE'), true)
            return false
        }
        if (els.uploadBtn && els.uploadBtn.disabled) {
            setUploadMessage(portalTranslate('T_MANUAL_BUSY'), true)
            return false
        }
        if (typeof JSZip === 'undefined') {
            setUploadMessage('JSZip not available.', true)
            return false
        }
        return true
    }

    const findManifestFile = (zip) => {
        const candidates = zip.file(/manifest\.json$/i)
        return candidates && candidates.length ? candidates[0] : null
    }

    const handleManualUpload = async () => {
        if (!ensureReadyForUpload()) return
        manualBusy = true
        if (els.uploadBtn) els.uploadBtn.disabled = true
        setUploadMessage('', false)

        try {
            const file = els.fileInput.files[0]
            const zip = await JSZip.loadAsync(file)
            const manifestEntry = findManifestFile(zip)
            if (!manifestEntry) throw new Error(portalTranslate('T_MANUAL_BAD_ZIP'))
            const manifestText = await manifestEntry.async('string')
            const manifest = JSON.parse(manifestText)
            const builds = manifest.builds || []
            if (!builds.length || !builds[0].parts || !builds[0].parts.length) {
                throw new Error(portalTranslate('T_MANUAL_BAD_ZIP'))
            }

            await sendPlain('/ota/upload/begin', manifestText)

            for (const part of builds[0].parts) {
                const partPath = part.path
                const entry = zip.file(partPath)
                if (!entry) throw new Error(`${partPath} missing in ZIP`)
                const data = new Uint8Array(await entry.async('uint8array'))
                await sendJson(
                    `/ota/upload/part/begin?path=${encodeURIComponent(partPath)}&offset=${encodeURIComponent(part.offset || 0)}&size=${data.length}`,
                    { method: 'POST' }
                )
                setUploadMessage(portalTranslate('T_MANUAL_UPLOADING', { path: partPath }))
                for (let i = 0; i < data.length; i += UPLOAD_CHUNK_SIZE) {
                    const slice = data.subarray(i, Math.min(i + UPLOAD_CHUNK_SIZE, data.length))
                    const base64 = chunkToBase64(slice)
                    await sendPlain('/ota/upload/part/chunk', base64)
                }
                await sendJson(`/ota/upload/part/finish?path=${encodeURIComponent(partPath)}`, { method: 'POST' })
            }

            await sendJson('/ota/upload/finish', { method: 'POST' })
            setUploadMessage(portalTranslate('T_MANUAL_DONE'), false)
        } catch (err) {
            console.error('OTA upload failed', err)
            setUploadMessage(err.message || portalTranslate('T_REMOTE_STATUS_ERROR'), true)
            try {
                await sendJson('/ota/cancel', { method: 'POST' })
            } catch (cancelErr) {
                console.warn('Failed to cancel OTA upload', cancelErr)
            }
        } finally {
            manualBusy = false
            if (els.uploadBtn) els.uploadBtn.disabled = false
        }
    }

    if (els.fetchBtn) {
        els.fetchBtn.addEventListener('click', triggerRemoteUpdate)
    }
    if (els.cancelBtn) {
        els.cancelBtn.addEventListener('click', cancelRemoteUpdate)
    }
    if (els.uploadBtn) {
        els.uploadBtn.addEventListener('click', handleManualUpload)
    }

    fetchStatus()
})()
