;(function () {
    const consoleEl = document.getElementById('logConsole')
    if (!consoleEl) return

    const statusEl = document.getElementById('logStatus')
    const autoScrollToggle = document.getElementById('autoScrollToggle')
    const copyBtn = document.getElementById('copyLogsButton')
    const clearBtn = document.getElementById('clearLogsButton')
    const translate = (key, params) => (typeof window.portalTranslate === 'function' ? window.portalTranslate(key, params) : key)
    let autoScroll = true
    let refreshHandle = null
    let isFetching = false

    function scheduleNextFetch(delay = 2000) {
        if (document.hidden) return
        if (refreshHandle) {
            clearTimeout(refreshHandle)
        }
        refreshHandle = setTimeout(fetchLogs, delay)
    }

    function setStatus(key, params = {}, isError = false) {
        if (!statusEl) return
        statusEl.textContent = key ? translate(key, params) : ''
        statusEl.classList.toggle('error', !!isError)
    }

    function render(lines) {
        const fragment = document.createDocumentFragment()
        lines.forEach((line) => {
            const div = document.createElement('div')
            div.className = 'log-line'
            div.textContent = line || '\u00A0'
            fragment.appendChild(div)
        })
        consoleEl.innerHTML = ''
        consoleEl.appendChild(fragment)
        if (autoScroll) {
            consoleEl.scrollTop = consoleEl.scrollHeight
        }
    }

    function fetchLogs() {
        if (isFetching) return
        isFetching = true
        fetch('/logs.json', { cache: 'no-store' })
            .then((resp) => {
                if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
                return resp.json()
            })
            .then((payload) => {
                const lines = Array.isArray(payload.lines) ? payload.lines : []
                render(lines)
                const time = new Date().toLocaleTimeString()
                setStatus('T_LOG_STATUS_UPDATED', { time })
            })
            .catch((err) => {
                console.warn('Log fetch failed', err)
                setStatus('T_LOG_STATUS_ERROR', {}, true)
            })
            .finally(() => {
                isFetching = false
                scheduleNextFetch()
            })
    }

    if (autoScrollToggle) {
        autoScrollToggle.addEventListener('change', () => {
            autoScroll = !!autoScrollToggle.checked
            if (autoScroll) {
                consoleEl.scrollTop = consoleEl.scrollHeight
            }
        })
    }

    if (copyBtn) {
        copyBtn.addEventListener('click', () => {
            const lines = Array.from(consoleEl.querySelectorAll('.log-line')).map((el) => el.textContent || '')
            const payload = lines.join('\n')
            const copyFallback = () => {
                const textarea = document.createElement('textarea')
                textarea.value = payload
                document.body.appendChild(textarea)
                textarea.select()
                try {
                    document.execCommand('copy')
                    setStatus('T_LOG_STATUS_COPIED')
                } catch (err) {
                    console.warn('Clipboard copy failed', err)
                    setStatus('T_LOG_STATUS_COPY_FAILED', {}, true)
                } finally {
                    document.body.removeChild(textarea)
                }
            }
            if (navigator.clipboard && navigator.clipboard.writeText) {
                navigator.clipboard
                    .writeText(payload)
                    .then(() => setStatus('T_LOG_STATUS_COPIED'))
                    .catch((err) => {
                        console.warn('Clipboard copy failed', err)
                        copyFallback()
                    })
            } else {
                copyFallback()
            }
        })
    }

    if (clearBtn) {
        clearBtn.addEventListener('click', () => {
            consoleEl.innerHTML = ''
            setStatus('T_LOG_STATUS_CLEARED')
        })
    }

    document.addEventListener('visibilitychange', () => {
        if (document.hidden) {
            if (refreshHandle) {
                clearTimeout(refreshHandle)
            }
            refreshHandle = null
        } else {
            fetchLogs()
        }
    })

    setStatus('T_LOG_STATUS_IDLE')
    fetchLogs()
})()
