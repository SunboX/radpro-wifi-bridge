// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
;(function () {
    const el = id => document.getElementById(id)
    const tr = key => {
        const value = typeof portalTranslate === 'function' ? portalTranslate(key) : ''
        return value === key ? '' : value
    }
    let state = null
    let uploading = false
    let initializedLocales = false
    let shownError = ''
    const selected = () => state?.locales?.find(item => item.locale === el('updateLocale').value)
    function refreshControls() {
        const locked = uploading || !state || state.busy || state.recoveryRequired
        // Keep recovery available if a detector disconnects during an update.
        const showInstallation = !!(state?.supported || state?.busy || state?.recoveryRequired || uploading)
        el('updateInstallation').hidden = !showInstallation
        el('updateScope').hidden = !showInstallation
        el('updateError').hidden = !!state && !showInstallation
        el('updateSupport').hidden = !el('updateSupport').textContent
        el('updateSupport').className = state && !state.supported ? 'update-support--unsupported' : ''
        el('updateLocale').disabled = locked || !state?.supported
        el('updateFile').disabled = locked || !state?.supported
        el('updateConfirm').disabled = locked
        el('updateStart').disabled = locked || !state?.supported || !el('updateConfirm').checked || !el('updateFile').files.length
        el('updateCancel').disabled = uploading || !!state?.busy || !!state?.recoveryRequired
        el('updateRetry').hidden = !state?.recoveryRequired || state.busy || state.uploaded !== state.expected
        el('updateRetry').disabled = uploading
        const image = selected()
        el('updateDownload').hidden = !image || locked
        if (image) {
            el('updateDownload').href = image.url
            el('updateDownload').download = image.filename
        }
    }
    async function post(action, fields = {}) {
        const response = await fetch('/radpro-update/' + action, {
            method: 'POST',
            body: new URLSearchParams({ csrf: el('csrfToken').value, ...fields }),
            cache: 'no-store'
        })
        const data = await response.json()
        if (!response.ok || !data.ok) throw new Error(data.error || 'Request failed')
    }
    async function poll() {
        try {
            const response = await fetch('/radpro-update/status', { cache: 'no-store' })
            if (!response.ok) throw new Error('Status request failed')
            state = await response.json()
            el('updateModel').textContent = state.model || '—'
            el('updateFirmware').textContent = state.firmware || '—'
            el('updateSupport').textContent = tr(state.supported ? 'T_RADPRO_SUPPORTED' : 'T_RADPRO_NOT_SUPPORTED')
            if (!initializedLocales && state.locales?.length) {
                for (const image of state.locales) {
                    const option = document.createElement('option')
                    option.value = option.textContent = image.locale
                    el('updateLocale').appendChild(option)
                }
                const locale = document.documentElement.lang === 'de' ? 'de' : 'en'
                if (state.locales.some(item => item.locale === locale)) el('updateLocale').value = locale
                initializedLocales = true
            }
            if (!uploading) {
                el('updateProgress').value = state.progress || 0
                el('updateStatus').textContent = tr('T_RADPRO_PHASE_' + state.phase.toUpperCase())
            }
            if (state.error) shownError = state.error
            el('updateError').textContent = shownError
        } catch (error) {
            // Fail closed when status is unavailable. Do not replace in-flight flash messages.
            state = null
            el('updateError').textContent = error.message
        }
        refreshControls()
    }
    async function install() {
        const image = selected()
        const file = el('updateFile').files[0]
        if (!state?.supported || state.busy || !el('updateConfirm').checked || !image || !file) return
        if (file.name !== image.filename || file.size !== image.size) {
            el('updateError').textContent = tr('T_RADPRO_WRONG_FILE')
            return
        }
        uploading = true
        shownError = ''
        el('updateError').textContent = ''
        refreshControls()
        try {
            await post('begin', { locale: image.locale })
            const bytes = new Uint8Array(await file.arrayBuffer())
            for (let offset = 0; offset < bytes.length; offset += 3072) {
                const block = bytes.subarray(offset, offset + 3072)
                const encoded = btoa(String.fromCharCode(...block))
                await post('chunk', { offset: String(offset), data: encoded })
                el('updateProgress').value = (offset + block.length) * 100 / bytes.length
                el('updateStatus').textContent = tr('T_RADPRO_PHASE_UPLOADING')
            }
            await post('start', { confirm: '3.1.1' })
        } catch (error) { shownError = error.message }
        finally { uploading = false; await poll() }
    }
    el('updateLocale').addEventListener('change', () => {
        el('updateFile').value = ''
        refreshControls()
    })
    el('updateFile').addEventListener('change', refreshControls)
    el('updateConfirm').addEventListener('change', refreshControls)
    el('updateStart').addEventListener('click', install)
    for (const [id, action] of [['updateCancel', 'cancel'], ['updateRetry', 'start']]) {
        el(id).addEventListener('click', async () => {
            uploading = true
            refreshControls()
            try { await post(action, action === 'start' ? { confirm: '3.1.1' } : {}); shownError = '' }
            catch (error) { shownError = error.message }
            finally { uploading = false; await poll() }
        })
    }
    poll()
    document.addEventListener('portal-locale-ready', poll)
    setInterval(poll, 1500)
})()
