const assert = require('node:assert/strict')
const fs = require('node:fs')
const path = require('node:path')

const root = path.join(__dirname, '..', '..')

function readText(relPath) {
    return fs.readFileSync(path.join(root, relPath), 'utf8')
}

function assertFormHasCsrfToken(relPath, action) {
    const html = readText(relPath)
    const actionIndex = html.indexOf(`action="${action}"`)
    assert.notEqual(actionIndex, -1, `${relPath} missing form action ${action}`)
    const formStart = html.lastIndexOf('<form', actionIndex)
    const formEnd = html.indexOf('</form>', actionIndex)
    assert.notEqual(formStart, -1, `${relPath} missing form start for ${action}`)
    assert.notEqual(formEnd, -1, `${relPath} missing form end for ${action}`)
    const form = html.slice(formStart, formEnd)
    assert.match(form, /name="csrf"/, `${relPath} ${action} form missing csrf field`)
    assert.match(form, /value="\{\{CSRF_TOKEN\}\}"/, `${relPath} ${action} form missing csrf template token`)
}

function testPostFormsIncludeCsrfToken() {
    assertFormHasCsrfToken('data/portal/mqtt.html', '/mqtt')
    assertFormHasCsrfToken('data/portal/osem.html', '/osem')
    assertFormHasCsrfToken('data/portal/radmon.html', '/radmon')
    assertFormHasCsrfToken('data/portal/gmc.html', '/gmc')
    assertFormHasCsrfToken('data/portal/safecast.html', '/safecast')
    assertFormHasCsrfToken('data/portal/backup.html', '/backup/restore')
}

function testOtaClientSendsCsrfToken() {
    const html = readText('data/portal/ota.html')
    assert.match(html, /name="csrf"/)
    assert.match(html, /value="\{\{CSRF_TOKEN\}\}"/)

    const js = readText('data/portal/js/ota-page.js')
    assert.match(js, /csrfToken/)
    assert.match(js, /csrf=/)
}

testPostFormsIncludeCsrfToken()
testOtaClientSendsCsrfToken()
console.log('portal CSRF asset tests passed')
