// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
const assert = require('node:assert/strict')
const fs = require('node:fs')
const path = require('node:path')
const vm = require('node:vm')
const source = fs.readFileSync(path.join(__dirname, '../../data/portal/js/radpro-update.js'), 'utf8')
const tick = () => new Promise(resolve => setImmediate(resolve))

async function main() {
    const ids = ['csrfToken', 'updateLocale', 'updateFile', 'updateConfirm', 'updateStart', 'updateCancel',
        'updateRetry', 'updateDownload', 'updateModel', 'updateFirmware', 'updateSupport', 'updateProgress', 'updateStatus', 'updateError',
        'updateInstallation', 'updateScope']
    const elements = Object.fromEntries(ids.map(id => [id, {
        value: '', files: [], checked: false, events: {}, children: [],
        addEventListener(name, fn) { this.events[name] = fn },
        appendChild(child) { this.children.push(child); if (!this.value) this.value = child.value }
    }]))
    elements.csrfToken.value = 'csrf-test-token'
    const image = { locale: 'en', size: 4, filename: 'radpro-bosean-fs600-en-3.1.1.bin', url: 'https://gissio.github.io/firmware.bin' }
    let state = { supported: true, model: 'Bosean FS-600', firmware: 'Rad Pro 3.0.2',
        busy: false, recoveryRequired: false, locales: [image], phase: 'idle', progress: 0, uploaded: 0, expected: 0 }
    let failStatus = false
    let poll
    const posts = []
    vm.runInNewContext(source, {
        document: { getElementById: id => elements[id], documentElement: { lang: 'en' }, createElement: () => ({}), addEventListener() {} },
        portalTranslate: key => 'translated:' + key, URLSearchParams, Uint8Array,
        btoa: value => Buffer.from(value, 'binary').toString('base64'),
        setInterval: fn => { poll = fn },
        fetch: async (url, options = {}) => {
            if (!options.method) {
                if (failStatus) throw new Error('Offline')
                return { ok: true, json: async () => state }
            }
            posts.push({ url, fields: Object.fromEntries(options.body.entries()) })
            return { ok: true, json: async () => ({ ok: true }) }
        }
    })
    await tick()
    assert.equal(elements.updateInstallation.hidden, false)
    assert.equal(elements.updateStart.disabled, true)
    elements.updateFile.files = [{ name: 'wrong.bin', size: 4 }]
    elements.updateConfirm.checked = true
    elements.updateConfirm.events.change()
    await elements.updateStart.events.click()
    assert.equal(posts.length, 0, 'Wrong filename must never start a server upload')
    assert.equal(elements.updateError.textContent, 'translated:T_RADPRO_WRONG_FILE')
    elements.updateFile.files = [{ name: image.filename, size: 4, arrayBuffer: async () => new Uint8Array([1, 2, 3, 4]).buffer }]
    await elements.updateStart.events.click()
    assert.deepEqual(posts.map(post => post.url), ['/radpro-update/begin', '/radpro-update/chunk', '/radpro-update/start'])
    assert(posts.every(post => post.fields.csrf === 'csrf-test-token'))
    assert.equal(posts[1].fields.offset, '0')
    assert.equal(posts[1].fields.data, 'AQIDBA==')
    assert.equal(posts[2].fields.confirm, '3.1.1')
    state = { ...state, busy: true, phase: 'writing' }
    await poll()
    assert.equal(elements.updateStart.disabled, true)
    assert.equal(elements.updateCancel.disabled, true)
    state = { ...state, busy: false, recoveryRequired: true, uploaded: 4, expected: 4, phase: 'failed' }
    await poll()
    assert.equal(elements.updateRetry.hidden, false)
    assert.equal(elements.updateStart.disabled, true)
    assert.equal(elements.updateCancel.disabled, true)
    assert.equal(elements.updateInstallation.hidden, false, 'Recovery controls must remain visible')
    state = { ...state, supported: false, recoveryRequired: false, locales: [] }
    await poll()
    assert.equal(elements.updateStart.disabled, true)
    assert.equal(elements.updateDownload.hidden, true)
    assert.equal(elements.updateInstallation.hidden, true)
    assert.equal(elements.updateScope.hidden, true)
    assert.equal(elements.updateError.hidden, true)
    assert.equal(elements.updateSupport.className, 'update-support--unsupported')
    state = { ...state, recoveryRequired: true }
    await poll()
    assert.equal(elements.updateInstallation.hidden, false, 'A disconnected detector must not hide active recovery')
    state = { ...state, supported: true, recoveryRequired: false, locales: [image] }
    await poll()
    assert.equal(elements.updateInstallation.hidden, false)
    assert.equal(elements.updateScope.hidden, false)
    assert.equal(elements.updateSupport.className, '')
    failStatus = true
    await poll()
    assert.equal(elements.updateStart.disabled, true)
    assert.equal(elements.updateError.textContent, 'Offline')
    assert.equal(elements.updateError.hidden, false)
    console.log('RadPro portal confirmation, upload, CSRF, busy and recovery checks passed')
}
main().catch(error => { console.error(error); process.exitCode = 1 })
