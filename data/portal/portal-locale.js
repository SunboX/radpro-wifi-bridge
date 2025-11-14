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
        active: {}
    }

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
            regex: /^RadPro WiFi Bridge - (.+)$/,
            key: 'T_PORTAL_SUBTITLE',
            params: (match) => ({ ip: match[1] })
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
        document.dispatchEvent(
            new CustomEvent('portal-locale-ready', {
                detail: { locale: state.locale }
            })
        )
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
