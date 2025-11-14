;(function () {
    const getEl = (id) => document.getElementById(id)

    document.addEventListener('DOMContentLoaded', () => {
        const fileInput = getEl('restoreFile')
        const hiddenField = getEl('configJsonField')
        const clientMsg = getEl('restoreClientMessage')
        const form = getEl('restoreForm')
        const submitBtn = getEl('restoreSubmit')

        if (!fileInput || !hiddenField || !clientMsg || !form || !submitBtn) return

        submitBtn.addEventListener('click', async () => {
            clientMsg.textContent = ''
            clientMsg.classList.remove('error')
            if (!fileInput.files.length) {
                clientMsg.textContent = portalTranslate('T_MSG_CHOOSE_FILE')
                clientMsg.classList.add('error')
                return
            }
            try {
                const text = await fileInput.files[0].text()
                hiddenField.value = text
                form.submit()
            } catch (err) {
                clientMsg.textContent = portalTranslate('T_MSG_READ_ERROR', { error: err.message })
                clientMsg.classList.add('error')
            }
        })
    })
})()
