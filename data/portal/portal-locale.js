;(function () {
    if (window.portalLocaleLoaderInitialized) {
        if (typeof window.portalApplyTranslations === 'function') {
            window.portalApplyTranslations()
        }
        return
    }
    window.portalLocaleLoaderInitialized = true

    const supportedLocales = ['en', 'de']

    const normalizeLocale = (value, fallback = 'en') => {
        if (!value || typeof value !== 'string') return fallback
        const cleaned = value.trim().toLowerCase()
        if (!cleaned) return fallback
        for (const locale of supportedLocales) {
            if (cleaned === locale || cleaned.startsWith(locale + '-')) return locale
        }
        return fallback
    }

    const localeMarker = document.querySelector('[data-portal-locale-marker="true"]')
    const portalHeadBootstrap = window.portalHeadBootstrap || null
    const requestedLocale = normalizeLocale(
        (typeof window.portalPreferredLocale === 'string' && window.portalPreferredLocale) ||
            (document.body && document.body.getAttribute('data-portal-locale-value')) ||
            (localeMarker && localeMarker.getAttribute('data-portal-locale-value')) ||
            document.documentElement.getAttribute('lang'),
        'en'
    )
    const state = {
        locale: 'en',
        fallback: {},
        active: {},
        bridgeFirmware: readInitialBridgeFirmware()
    }

    const normalizeSubtitleLabel = (value) => {
        if (!value || typeof value !== 'string') return ''
        return value.trim()
    }

    function readInitialBridgeFirmware() {
        if (!portalHeadBootstrap) return ''
        if (typeof portalHeadBootstrap.readCachedVersion === 'function') {
            return normalizeSubtitleLabel(portalHeadBootstrap.readCachedVersion())
        }
        return normalizeSubtitleLabel(portalHeadBootstrap.state && portalHeadBootstrap.state.version)
    }

    function writeBridgeFirmwareCache(version) {
        const normalized = normalizeSubtitleLabel(version)
        if (!normalized || !portalHeadBootstrap || typeof portalHeadBootstrap.writeCachedVersion !== 'function') {
            return normalized
        }
        return normalizeSubtitleLabel(portalHeadBootstrap.writeCachedVersion(normalized))
    }

    function isPortalSubtitlePending() {
        if (portalHeadBootstrap && portalHeadBootstrap.state && portalHeadBootstrap.state.pending) return true
        return !!(document.documentElement && document.documentElement.classList && document.documentElement.classList.contains('portal-subtitle-pending'))
    }

    function markPortalSubtitleReady() {
        if (portalHeadBootstrap && typeof portalHeadBootstrap.markReady === 'function') {
            portalHeadBootstrap.markReady()
            return
        }
        if (!document.documentElement || !document.documentElement.classList) return
        document.documentElement.classList.remove('portal-subtitle-pending')
        document.documentElement.classList.add('portal-subtitle-ready')
    }

    const portalLocaleChrome = {
        extractPortalSubtitleLabel(text) {
            const trimmed = normalizeSubtitleLabel(text)
            if (!trimmed || !trimmed.startsWith('RadPro WiFi Bridge')) return ''

            let label = trimmed.slice('RadPro WiFi Bridge'.length).trim()
            if (!label) return ''
            if (label.startsWith('-')) {
                label = label.slice(1).trim()
            }
            if (!label || label === 'Configuration' || label === 'Konfiguration') return ''
            return label
        },

        buildPortalSubtitle(translate, label) {
            const normalized = normalizeSubtitleLabel(label)
            if (!normalized || typeof translate !== 'function') return ''
            return translate('T_PORTAL_SUBTITLE', { version: normalized, ip: normalized })
        },

        applyPortalSubtitle(element, translate, version, options = {}) {
            if (!element) return ''

            const dataset = element.dataset || {}
            const allowExistingLabel = options.allowExistingLabel !== false
            const label =
                normalizeSubtitleLabel(version) ||
                normalizeSubtitleLabel(dataset.portalSubtitleLabel) ||
                (allowExistingLabel ? this.extractPortalSubtitleLabel(element.textContent) : '')
            if (!label) return ''

            dataset.portalSubtitleLabel = label
            const translated = this.buildPortalSubtitle(translate, label)
            if (translated) {
                element.textContent = translated
            }
            return translated
        }
    }

    window.portalLocaleChrome = portalLocaleChrome

    window.portalTranslate = function translate(key, params = {}) {
        let text = state.active[key] || state.fallback[key] || key
        for (const [token, value] of Object.entries(params)) {
            const pattern = new RegExp(`\\{${token}\\}`, 'g')
            text = text.replace(pattern, value)
        }
        return text
    }

    window.portalApplyTranslations = applyTranslations

    document.addEventListener('DOMContentLoaded', () => {
        loadLocale('en', true)
            .then(() => {
                if (requestedLocale !== 'en') {
                    return loadLocale(requestedLocale, false)
                }
                return detectDeviceLocale().then((detected) => {
                    if (detected && detected !== 'en') {
                        return loadLocale(detected, false)
                    }
                    return null
                })
            })
            .catch((err) => {
                console.warn('Localization load failed', err)
            })
            .finally(() => {
                if (!Object.keys(state.active).length) {
                    state.active = state.fallback
                }
                applyTranslations()
                loadBridgeFirmwareVersion()
            })
    })

    function loadLocale(locale, isFallback) {
        locale = normalizeLocale(locale, 'en')
        return fetch(`/portal/locales/${locale}.json`, { cache: 'no-store' })
            .then((resp) => {
                if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
                return resp.json()
            })
            .then((dict) => {
                if (isFallback) {
                    state.fallback = dict
                    if (!Object.keys(state.active).length) {
                        state.active = dict
                        state.locale = locale
                    }
                } else {
                    state.active = dict
                    state.locale = locale
                }
            })
            .catch((err) => {
                console.warn('Failed to load locale', locale, err)
                if (!isFallback && locale !== 'en') {
                    state.active = state.fallback
                }
            })
    }

    function detectDeviceLocale() {
        return fetch('/device.json', { cache: 'no-store' })
            .then((resp) => {
                if (!resp.ok) return null
                return resp.json()
            })
            .then((data) => {
                if (!data || typeof data.locale !== 'string') return null
                return normalizeLocale(data.locale, null)
            })
            .catch(() => null)
    }

    const WIFI_MANAGER_TEXT_PATTERNS = [
        {
            regex: /^RadPro WiFi Bridge Configuration$/,
            key: 'T_PORTAL_HEADING'
        },
        {
            regex: /^Configure WiFi$/,
            key: 'T_PORTAL_WIFI_BUTTON'
        },
        {
            regex: /^Connected to (.+)$/,
            key: 'T_PORTAL_CONNECTED',
            params: (match) => ({ ssid: match[1] })
        },
        {
            regex: /^with IP (.+)$/,
            key: 'T_PORTAL_IP',
            params: (match) => ({ ip: match[1] })
        }
    ]

    function applyTranslations() {
        document.querySelectorAll('[data-i18n]').forEach((el) => {
            if (!el.dataset.i18nDefault) {
                el.dataset.i18nDefault = el.textContent
            }
            const key = el.getAttribute('data-i18n')
            const text = portalTranslate(key)
            if (text) el.textContent = text
        })

        document.querySelectorAll('[data-i18n-html]').forEach((el) => {
            if (!el.dataset.i18nDefault) {
                el.dataset.i18nDefault = el.innerHTML
            }
            const key = el.getAttribute('data-i18n-html')
            const text = portalTranslate(key)
            if (text) el.innerHTML = text
        })

        if (document.body && document.body.dataset.i18nTitle) {
            document.title = portalTranslate(document.body.dataset.i18nTitle)
        }

        translateWifiManagerChrome()
        refreshPortalSubtitle(state.bridgeFirmware, { allowExistingLabel: !isPortalSubtitlePending() })
        document.dispatchEvent(
            new CustomEvent('portal-locale-ready', {
                detail: { locale: state.locale }
            })
        )
    }

    function findPortalSubtitleElement() {
        const wrap = document.querySelector('.wrap')
        if (!wrap || typeof wrap.querySelector !== 'function') return null
        const subtitle = wrap.querySelector('h3')
        if (!subtitle) return null
        if (!portalLocaleChrome.extractPortalSubtitleLabel(subtitle.textContent) && !(subtitle.dataset && subtitle.dataset.portalSubtitleLabel)) {
            return null
        }
        return subtitle
    }

    function refreshPortalSubtitle(version, options = {}) {
        const subtitle = findPortalSubtitleElement()
        if (!subtitle) return ''
        const translated = portalLocaleChrome.applyPortalSubtitle(subtitle, portalTranslate, version, options)
        if (translated) {
            markPortalSubtitleReady()
        }
        return translated
    }

    function loadBridgeFirmwareVersion() {
        if (!findPortalSubtitleElement()) return Promise.resolve(null)

        return fetch('/bridge.json', { cache: 'no-store' })
            .then((resp) => {
                if (!resp.ok) return null
                return resp.json()
            })
            .then((data) => {
                const version = writeBridgeFirmwareCache(data && data.bridgeFirmware)
                if (!version) return null
                state.bridgeFirmware = version
                refreshPortalSubtitle(version, { allowExistingLabel: false })
                return version
            })
            .catch((err) => {
                console.warn('Bridge firmware lookup failed', err)
                return null
            })
    }

    function translateWifiManagerChrome() {
        const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, null)
        const candidates = []
        let node
        while ((node = walker.nextNode())) {
            if (!node.nodeValue) continue
            const trimmed = node.nodeValue.trim()
            if (!trimmed) continue
            candidates.push({ node, original: node.nodeValue, trimmed })
        }

        candidates.forEach(({ node, original, trimmed }) => {
            for (const pattern of WIFI_MANAGER_TEXT_PATTERNS) {
                const match = pattern.regex.exec(trimmed)
                if (!match) continue
                const params = pattern.params ? pattern.params(match) : {}
                const replacement = portalTranslate(pattern.key, params)
                if (!replacement) break
                const start = original.indexOf(trimmed)
                const leading = start >= 0 ? original.slice(0, start) : ''
                const trailing = start >= 0 ? original.slice(start + trimmed.length) : ''
                node.nodeValue = leading + replacement + trailing
                break
            }
        })

        document.querySelectorAll('.msg strong').forEach((el) => {
            const tail = el.nextSibling
            if (!tail || tail.nodeType !== Node.TEXT_NODE) return
            const match = tail.textContent.match(/^\s*to\s+(.*)$/i)
            if (!match) return
            const ssid = match[1]
            const translated = portalTranslate('T_PORTAL_CONNECTED', { ssid })
            if (translated && translated !== 'T_PORTAL_CONNECTED') {
                el.textContent = translated
                tail.textContent = ''
            }
        })
    }
})()
