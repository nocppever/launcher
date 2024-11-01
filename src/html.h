const char* uploadHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>M5Stack Firmware Manager</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        .card { background: #f0f0f0; padding: 20px; margin: 10px 0; border-radius: 5px; }
        .button { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }
        .progress { width: 100%; height: 20px; background: #ddd; border-radius: 5px; margin: 10px 0; }
        .progress-bar { width: 0%; height: 100%; background: #007bff; border-radius: 5px; transition: width 0.3s; }
    </style>
</head>
<body>
    <div class="container">
        <h1>M5Stack Firmware Manager</h1>
        
        <div class="card">
            <h2>Upload New Firmware</h2>
            <form id="uploadForm">
                <p>
                    <label>Firmware Name:</label><br>
                    <input type="text" id="name" required>
                </p>
                <p>
                    <label>Version:</label><br>
                    <input type="text" id="version" required>
                </p>
                <p>
                    <label>Description:</label><br>
                    <textarea id="description" rows="3"></textarea>
                </p>
                <p>
                    <label>Firmware Binary:</label><br>
                    <input type="file" id="file" accept=".bin" required>
                </p>
                <button type="submit" class="button">Upload Firmware</button>
            </form>
            <div class="progress">
                <div class="progress-bar" id="progressBar"></div>
            </div>
        </div>

        <div class="card">
            <h2>Installed Firmware</h2>
            <div id="firmwareList"></div>
        </div>
    </div>

    <script>
        document.getElementById('uploadForm').onsubmit = async function(e) {
            e.preventDefault();
            
            const formData = new FormData();
            formData.append('name', document.getElementById('name').value);
            formData.append('version', document.getElementById('version').value);
            formData.append('description', document.getElementById('description').value);
            formData.append('file', document.getElementById('file').files[0]);

            try {
                const response = await fetch('/upload', {
                    method: 'POST',
                    body: formData
                });
                
                if (response.ok) {
                    alert('Firmware uploaded successfully');
                    loadFirmwareList();
                } else {
                    alert('Upload failed');
                }
            } catch (error) {
                alert('Upload failed: ' + error);
            }
        };

        function updateFirmware(name) {
            fetch('/update-firmware?name=' + encodeURIComponent(name))
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('Firmware update initiated');
                    } else {
                        alert('Update failed: ' + data.error);
                    }
                })
                .catch(error => alert('Update failed: ' + error));
        }

        function createFirmwareCard(fw) {
            var card = document.createElement('div');
            card.className = 'card';
            card.innerHTML = 
                '<h3>' + fw.name + ' (v' + fw.version + ')</h3>' +
                '<p>' + fw.description + '</p>' +
                '<button onclick="updateFirmware(\'' + fw.name + '\')" class="button">Update</button>';
            return card;
        }

        function loadFirmwareList() {
            fetch('/firmware-list')
                .then(response => response.json())
                .then(firmware => {
                    const list = document.getElementById('firmwareList');
                    list.innerHTML = '';
                    firmware.forEach(fw => {
                        list.appendChild(createFirmwareCard(fw));
                    });
                })
                .catch(error => console.error('Error loading firmware list:', error));
        }

        loadFirmwareList();
    </script>
</body>
</html>
)rawliteral";