const assert = require('node:assert/strict')
const fs = require('node:fs')
const path = require('node:path')
const vm = require('node:vm')

function loadPortalLocaleChrome() {
    const scriptPath = path.join(__dirname, '..', '..', 'data', 'portal', 'portal-locale.js')
    const source = fs.readFileSync(scriptPath, 'utf8')
    const sandbox = {
        console,
        fetch: () => Promise.reject(new Error('fetch not expected in unit test')),
        setTimeout: () => 0,
        clearTimeout: () => {},
        CustomEvent: class CustomEvent {
            constructor(name, init = {}) {
                this.type = name
                this.detail = init.detail
            }
        },
        NodeFilter: { SHOW_TEXT: 4 }
    }

    sandbox.window = sandbox
    sandbox.document = {
        body: {
            dataset: {},
            getAttribute: () => 'en'
        },
        documentElement: {
            getAttribute: () => 'en'
        },
        querySelector: () => null,
        querySelectorAll: () => [],
        addEventListener: () => {},
        dispatchEvent: () => {}
    }

    vm.createContext(sandbox)
    vm.runInContext(source, sandbox, { filename: 'portal-locale.js' })
    return sandbox.window.portalLocaleChrome
}

function translate(key, params = {}) {
    if (key !== 'T_PORTAL_SUBTITLE') return key
    return `RadPro WiFi Bridge ${params.version}`
}

function testReplacesDashSeparatedIpWithVersion() {
    const chrome = loadPortalLocaleChrome()
    assert.ok(chrome, 'expected portal subtitle helpers to be exposed')

    const subtitle = {
        textContent: 'RadPro WiFi Bridge - 192.168.1.59',
        dataset: {}
    }

    const text = chrome.applyPortalSubtitle(subtitle, translate, '1.15.6')
    assert.equal(text, 'RadPro WiFi Bridge 1.15.6')
    assert.equal(subtitle.textContent, 'RadPro WiFi Bridge 1.15.6')
    assert.equal(subtitle.dataset.portalSubtitleLabel, '1.15.6')
}

function testFallsBackToExistingLabelWhenVersionMissing() {
    const chrome = loadPortalLocaleChrome()
    const subtitle = {
        textContent: 'RadPro WiFi Bridge - 192.168.1.59',
        dataset: {}
    }

    const text = chrome.applyPortalSubtitle(subtitle, translate, '')
    assert.equal(text, 'RadPro WiFi Bridge 192.168.1.59')
    assert.equal(subtitle.textContent, 'RadPro WiFi Bridge 192.168.1.59')
    assert.equal(subtitle.dataset.portalSubtitleLabel, '192.168.1.59')
}

function testCanSkipExistingIpFallbackWhileBootstrapPending() {
    const chrome = loadPortalLocaleChrome()
    const subtitle = {
        textContent: 'RadPro WiFi Bridge - 192.168.1.59',
        dataset: {}
    }

    const text = chrome.applyPortalSubtitle(subtitle, translate, '', { allowExistingLabel: false })
    assert.equal(text, '')
    assert.equal(subtitle.textContent, 'RadPro WiFi Bridge - 192.168.1.59')
    assert.equal(subtitle.dataset.portalSubtitleLabel, undefined)
}

testReplacesDashSeparatedIpWithVersion()
testFallsBackToExistingLabelWhenVersionMissing()
testCanSkipExistingIpFallbackWhileBootstrapPending()
console.log('portal locale subtitle tests passed')
