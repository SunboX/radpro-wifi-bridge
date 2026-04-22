;(function () {
    if (window.portalHeadBootstrap) return

    const CACHE_KEY = 'radproBridgeFirmwareVersion'
    const BRAND = 'RadPro WiFi Bridge'
    const PENDING_CLASS = 'portal-subtitle-pending'
    const READY_CLASS = 'portal-subtitle-ready'
    const FALLBACK_REVEAL_MS = 2500

    const normalizeVersion = (value) => {
        if (!value || typeof value !== 'string') return ''
        return value.trim()
    }

    const isRootPortalPage = () => {
        const path = (typeof location === 'object' && location && typeof location.pathname === 'string' && location.pathname) || '/'
        return path === '/' || path === ''
    }

    const markPending = () => {
        const root = document.documentElement
        if (root && root.classList) {
            root.classList.add(PENDING_CLASS)
            root.classList.remove(READY_CLASS)
        }
        state.pending = true
    }

    const markReady = () => {
        const root = document.documentElement
        if (root && root.classList) {
            root.classList.remove(PENDING_CLASS)
            root.classList.add(READY_CLASS)
        }
        state.pending = false
    }

    const readCachedVersion = () => {
        try {
            return normalizeVersion(localStorage.getItem(CACHE_KEY))
        } catch (err) {
            return ''
        }
    }

    const writeCachedVersion = (value) => {
        const version = normalizeVersion(value)
        if (!version) return ''
        state.version = version
        try {
            localStorage.setItem(CACHE_KEY, version)
        } catch (err) {}
        return version
    }

    const applyVersionToSubtitle = (element, value) => {
        const version = normalizeVersion(value)
        if (!element || !version) return false
        element.textContent = `${BRAND} ${version}`
        if (element.dataset) {
            element.dataset.portalSubtitleLabel = version
        }
        markReady()
        return true
    }

    const findSubtitle = () => {
        if (!document || typeof document.querySelector !== 'function') return null
        return document.querySelector('.wrap > h3')
    }

    const state = {
        version: readCachedVersion(),
        pending: false
    }

    window.portalHeadBootstrap = {
        cacheKey: CACHE_KEY,
        state,
        readCachedVersion,
        writeCachedVersion,
        applyVersionToSubtitle,
        markPending,
        markReady
    }

    if (!isRootPortalPage()) return

    markPending()

    const tryApplyKnownVersion = () => {
        const subtitle = findSubtitle()
        if (!subtitle || !state.version) return false
        return applyVersionToSubtitle(subtitle, state.version)
    }

    let observer = null
    if (!tryApplyKnownVersion() && typeof MutationObserver === 'function' && document && document.documentElement) {
        observer = new MutationObserver(() => {
            if (tryApplyKnownVersion() && observer) {
                observer.disconnect()
                observer = null
            }
        })
        observer.observe(document.documentElement, { childList: true, subtree: true })
    }

    const fallbackTimer =
        typeof setTimeout === 'function'
            ? setTimeout(() => {
                  if (state.pending) markReady()
              }, FALLBACK_REVEAL_MS)
            : 0

    if (typeof fetch !== 'function') return

    fetch('/bridge.json', { cache: 'no-store' })
        .then((resp) => {
            if (!resp.ok) return null
            return resp.json()
        })
        .then((data) => {
            const version = writeCachedVersion(data && data.bridgeFirmware)
            if (!version) return
            const subtitle = findSubtitle()
            if (subtitle) {
                applyVersionToSubtitle(subtitle, version)
            }
            if (observer) {
                observer.disconnect()
                observer = null
            }
            if (fallbackTimer && typeof clearTimeout === 'function') {
                clearTimeout(fallbackTimer)
            }
        })
        .catch(() => {})
})()
