// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

const assert = require('node:assert/strict')
const fs = require('node:fs')
const path = require('node:path')
const vm = require('node:vm')

function loadPortalHeadBootstrap(localStorageValues = {}) {
    const scriptPath = path.join(__dirname, '..', '..', 'data', 'portal', 'portal-head-bootstrap.js')
    const source = fs.readFileSync(scriptPath, 'utf8')

    const localStorage = {
        getItem(key) {
            return Object.prototype.hasOwnProperty.call(localStorageValues, key) ? localStorageValues[key] : null
        },
        setItem(key, value) {
            localStorageValues[key] = String(value)
        }
    }

    const documentElement = {
        classList: {
            added: [],
            removed: [],
            add(name) {
                this.added.push(name)
            },
            remove(name) {
                this.removed.push(name)
            }
        }
    }

    const sandbox = {
        console,
        localStorage,
        location: { pathname: '/' },
        document: {
            documentElement,
            querySelector: () => null
        },
        MutationObserver: class MutationObserver {
            constructor(cb) {
                this.cb = cb
            }
            observe() {}
            disconnect() {}
        }
    }

    sandbox.window = sandbox
    vm.createContext(sandbox)
    vm.runInContext(source, sandbox, { filename: 'portal-head-bootstrap.js' })
    return sandbox.window.portalHeadBootstrap
}

function testUsesCachedVersionToReplaceIpSubtitle() {
    const bootstrap = loadPortalHeadBootstrap({
        [loadPortalHeadBootstrap().cacheKey]: '1.15.6'
    })
    const subtitle = {
        textContent: 'RadPro WiFi Bridge - 192.168.1.59',
        dataset: {}
    }

    const changed = bootstrap.applyVersionToSubtitle(subtitle, '1.15.6')
    assert.equal(changed, true)
    assert.equal(subtitle.textContent, 'RadPro WiFi Bridge 1.15.6')
    assert.equal(subtitle.dataset.portalSubtitleLabel, '1.15.6')
}

function testReadsCachedVersionFromLocalStorage() {
    const bootstrap = loadPortalHeadBootstrap({
        radproBridgeFirmwareVersion: '1.15.6'
    })
    assert.equal(bootstrap.readCachedVersion(), '1.15.6')
}

testUsesCachedVersionToReplaceIpSubtitle()
testReadsCachedVersionFromLocalStorage()
console.log('portal head bootstrap tests passed')
