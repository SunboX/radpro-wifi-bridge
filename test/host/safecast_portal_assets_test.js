const assert = require('node:assert/strict')
const fs = require('node:fs')
const path = require('node:path')

const root = path.join(__dirname, '..', '..')

function readJson(relPath) {
    return JSON.parse(fs.readFileSync(path.join(root, relPath), 'utf8'))
}

function readText(relPath) {
    return fs.readFileSync(path.join(root, relPath), 'utf8')
}

function testMenuIncludesSafecastButton() {
    const html = readText('data/portal/menu.html')
    assert.match(html, /action="\/safecast"/)
    assert.match(html, /T_MENU_CONFIGURE_SAFECAST/)
}

function testLocalesIncludeSafecastStrings() {
    const en = readJson('data/portal/locales/en.json')
    const de = readJson('data/portal/locales/de.json')

    assert.equal(en.T_MENU_CONFIGURE_SAFECAST, 'Configure Safecast')
    assert.equal(de.T_MENU_CONFIGURE_SAFECAST, 'Safecast konfigurieren')
    assert.ok(en.T_SAFECAST_OPEN_DATA_WARNING.includes('open data'))
    assert.ok(de.T_SAFECAST_OPEN_DATA_WARNING.includes('offene Datenplattform'))
}

function testSafecastTemplateContainsRequiredControls() {
    const html = readText('data/portal/safecast.html')

    assert.match(html, /name="safecastEnabled"/)
    assert.match(html, /name="safecastApiKey"/)
    assert.match(html, /name="safecastDeviceId"/)
    assert.match(html, /name="safecastLatitude"/)
    assert.match(html, /name="safecastLongitude"/)
    assert.match(html, /name="safecastHeightCm"/)
    assert.match(html, /name="safecastLocationName"/)
    assert.match(html, /name="safecastUnit"/)
    assert.match(html, /name="safecastUploadIntervalSeconds"/)
    assert.match(html, /name="safecastUseTestApi"/)
    assert.match(html, /name="safecastCustomApiBaseUrl"/)
    assert.match(html, /name="safecastDebug"/)
    assert.match(html, /value="test"/)
    assert.match(html, /value="save"/)
}

testMenuIncludesSafecastButton()
testLocalesIncludeSafecastStrings()
testSafecastTemplateContainsRequiredControls()
console.log('safecast portal asset tests passed')
