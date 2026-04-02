const nodeGrid = document.getElementById('node-grid');

// Mapping of nodes to avoid destroying standard DOM elements
let existingNodes = new Set();

async function fetchRegistryState() {
    try {
        // Fetch from the C++ API we added on port 9002
        const response = await fetch('http://localhost:9002/api/state');
        if (!response.ok) throw new Error('API down');
        
        const nodes = await response.json();
        
        if (nodes.length === 0) {
            nodeGrid.innerHTML = '<div class="loading">No active nodes connected...</div>';
            existingNodes.clear();
            return;
        }

        // If nodes transitioned from 0 to >0, clear the loading text
        if (existingNodes.size === 0) {
            nodeGrid.innerHTML = '';
        }

        const currentIds = new Set();

        nodes.forEach(node => {
            currentIds.add(node.id);
            const domId = 'node-' + node.id.replace(':', '-').replace('.', '-');

            let card = document.getElementById(domId);
            if (!card) {
                // Generate UI
                card = document.createElement('div');
                card.id = domId;
                card.className = 'node-card';
                
                card.innerHTML = `
                    <div class="node-header">
                        <div class="node-id">${node.ip}:${node.port}</div>
                        <div class="node-score" id="score-${domId}">0.0</div>
                    </div>
                    
                    <div class="metric">
                        <div class="metric-header">
                            <span>CPU Usage</span>
                            <span id="cputxt-${domId}">0%</span>
                        </div>
                        <div class="progress-bg">
                            <div class="progress-bar" id="cpubar-${domId}" style="width: 0%"></div>
                        </div>
                    </div>

                    <div class="metric">
                        <div class="metric-header">
                            <span>RAM Usage</span>
                            <span id="ramtxt-${domId}">0%</span>
                        </div>
                        <div class="progress-bg">
                            <div class="progress-bar" id="rambar-${domId}" style="width: 0%"></div>
                        </div>
                    </div>
                `;
                nodeGrid.appendChild(card);
                existingNodes.add(node.id);
            }

            // Status category
            card.classList.remove('healthy', 'warning', 'critical');
            if (node.score < 40) card.classList.add('healthy');
            else if (node.score < 75) card.classList.add('warning');
            else card.classList.add('critical');

            // Update DOM Values dynamically
            document.getElementById(`score-${domId}`).innerText = node.score.toFixed(1);
            
            document.getElementById(`cputxt-${domId}`).innerText = node.cpu.toFixed(1) + '%';
            document.getElementById(`cpubar-${domId}`).style.width = node.cpu + '%';
            
            document.getElementById(`ramtxt-${domId}`).innerText = node.ram.toFixed(1) + '%';
            document.getElementById(`rambar-${domId}`).style.width = node.ram + '%';
        });

        // Remove dead nodes
        for (let oldId of existingNodes) {
            if (!currentIds.has(oldId)) {
                const domId = 'node-' + oldId.replace(':', '-').replace('.', '-');
                const card = document.getElementById(domId);
                if (card) card.remove();
                existingNodes.delete(oldId);
            }
        }

    } catch (err) {
        nodeGrid.innerHTML = `
            <div class="loading" style="color: #ef4444;">
                Cannot connect to Registry API.<br>
                Make sure registry.exe is running!
            </div>
        `;
        existingNodes.clear();
    }
}

// Poll every 1 second
setInterval(fetchRegistryState, 1000);
fetchRegistryState();
