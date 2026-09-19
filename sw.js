// =====================================================================
// SERVICE WORKER - AUTO-UPDATE & OFFLINE CACHE
// ESP32 NEXUS CORE PWA
// Version: esp32-nexus-core-v6
// =====================================================================

const CACHE_NAME = 'esp32-nexus-core-v6';

const ASSETS_TO_CACHE = [
  './',
  './index.html',
  './manifest.json'
];

// 1. CÀI ĐẶT: Cache các tài nguyên cốt lõi và bỏ qua chờ đợi (skipWaiting)
self.addEventListener('install', (event) => {
  console.log('[SW] Đang cài đặt phiên bản mới:', CACHE_NAME);
  self.skipWaiting(); // Ngay lập tức kích hoạt bản mới mà không đợi đóng tab

  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      console.log('[SW] Pre-caching offline core assets');
      return cache.addAll(ASSETS_TO_CACHE);
    })
  );
});

// 2. KÍCH HOẠT: Dọn dẹp toàn bộ cache cũ và lập tức kiểm soát mọi client (clients.claim)
self.addEventListener('activate', (event) => {
  console.log('[SW] Đang kích hoạt phiên bản mới:', CACHE_NAME);
  event.waitUntil(
    caches.keys().then((cacheNames) => {
      return Promise.all(
        cacheNames.map((cache) => {
          if (cache !== CACHE_NAME) {
            console.log('[SW] Đang dọn dẹp cache cũ:', cache);
            return caches.delete(cache);
          }
        })
      );
    }).then(() => {
      // Tiếp quản quyền điều khiển tất cả các tab / standalone PWA đang mở
      return self.clients.claim();
    })
  );
});

// 3. FETCH: Chiến lược Network-First cho Navigation (để luôn tự động nhận code mới)
//            và Cache-First cho tài sản tĩnh khi offline.
self.addEventListener('fetch', (event) => {
  // Bỏ qua các request không phải GET
  if (event.request.method !== 'GET') return;

  const requestUrl = new URL(event.request.url);

  // Không can thiệp vào các request nội mạng ESP32 hoặc API động
  if (
    requestUrl.hostname.endsWith('.local') ||
    requestUrl.pathname.startsWith('/api/') ||
    requestUrl.hostname.startsWith('192.168.') ||
    requestUrl.protocol === 'ws:' ||
    requestUrl.protocol === 'wss:'
  ) {
    return;
  }

  // Đối với request trang HTML chính (Navigate): Ưu tiên mạng (Network-First)
  // để người dùng mở app là luôn nhận phiên bản mới nhất trên GitHub Pages
  if (event.request.mode === 'navigate' || event.request.destination === 'document') {
    event.respondWith(
      fetch(event.request)
        .then((networkResponse) => {
          if (networkResponse && networkResponse.status === 200) {
            const copy = networkResponse.clone();
            caches.open(CACHE_NAME).then((cache) => cache.put(event.request, copy));
          }
          return networkResponse;
        })
        .catch(() => {
          // Khi offline hoàn toàn, phục vụ bản cached
          return caches.match('./index.html');
        })
    );
    return;
  }

  // Đối với các tài nguyên tĩnh còn lại: Cache-first, fallback mạng
  event.respondWith(
    caches.match(event.request).then((cachedResponse) => {
      if (cachedResponse) {
        return cachedResponse;
      }
      return fetch(event.request).then((networkResponse) => {
        if (!networkResponse || networkResponse.status !== 200 || networkResponse.type !== 'basic') {
          return networkResponse;
        }
        const copy = networkResponse.clone();
        caches.open(CACHE_NAME).then((cache) => cache.put(event.request, copy));
        return networkResponse;
      });
    })
  );
});
