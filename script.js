let enabled = true;
let currentTab = null;
let ws = null;

// Load enabled state
chrome.storage.local.get(['enabled'], (result) => {
  enabled = result.enabled !== false;
  if (enabled) connectWebSocket();
});

// Connect to bridge via WebSocket
function connectWebSocket() {
  if (ws && ws.readyState === WebSocket.OPEN) return;
  
  ws = new WebSocket('ws://localhost:8947');
  
  ws.onopen = () => {
    console.log('Connected to HwBrowserPresence bridge');
    updatePresence();
  };
  
  ws.onclose = () => {
    console.log('Disconnected from bridge');
    ws = null;
    if (enabled) {
      setTimeout(connectWebSocket, 5000);
    }
  };
  
  ws.onerror = (err) => {
    console.error('WebSocket error:', err);
  };
}

// Update presence with current tab info
function updatePresence() {
  if (!enabled || !ws || ws.readyState !== WebSocket.OPEN) return;
  
  chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
    if (tabs[0]) {
      const tab = tabs[0];
      const url = new URL(tab.url || 'about:blank');
      
      const data = {
        title: tab.title || 'Browser',
        domain: url.hostname || 'browser',
        favicon: tab.favIconUrl || ''
      };
      
      ws.send(JSON.stringify(data));
      currentTab = tab.id;
    }
  });
}

// Listen for tab changes
chrome.tabs.onActivated.addListener(() => {
  updatePresence();
});

chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
  if (tabId === currentTab && changeInfo.status === 'complete') {
    updatePresence();
  }
});

// Listen for enable/disable messages
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.action === 'toggle') {
    enabled = request.enabled;
    chrome.storage.local.set({ enabled });
    
    if (enabled) {
      connectWebSocket();
    } else if (ws) {
      ws.close();
      ws = null;
    }
    
    sendResponse({ success: true });
  } else if (request.action === 'getState') {
    sendResponse({ enabled });
  }
  return true;
});
