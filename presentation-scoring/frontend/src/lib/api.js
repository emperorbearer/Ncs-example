# VITE_API_URL 환경변수로 외부 백엔드 주소 지정 가능 (기본: 같은 서버의 /api)
const BASE = import.meta.env.VITE_API_URL || '/api';

function getToken() {
  if (typeof localStorage === 'undefined') return null;
  return localStorage.getItem('token');
}

async function req(path, opts = {}) {
  const token = getToken();
  const res = await fetch(`${BASE}${path}`, {
    ...opts,
    headers: {
      'Content-Type': 'application/json',
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
      ...opts.headers
    }
  });
  if (!res.ok) {
    const err = await res.json().catch(() => ({ detail: '오류가 발생했습니다' }));
    throw new Error(err.detail || '오류가 발생했습니다');
  }
  return res.json();
}

export const api = {
  // Auth
  login: (d) => req('/auth/login', { method: 'POST', body: JSON.stringify(d) }),
  register: (d) => req('/auth/register', { method: 'POST', body: JSON.stringify(d) }),
  getMe: () => req('/auth/me'),

  // Classes
  getClasses: () => req('/classes/'),
  createClass: (d) => req('/classes/', { method: 'POST', body: JSON.stringify(d) }),
  getClass: (id) => req(`/classes/${id}`),
  deleteClass: (id) => req(`/classes/${id}`, { method: 'DELETE' }),
  getClassQR: (id, baseUrl) =>
    req(`/classes/${id}/qr?base_url=${encodeURIComponent(baseUrl)}`),
  uploadStudents: (classId, file) => {
    const token = getToken();
    const form = new FormData();
    form.append('file', file);
    return fetch(`${BASE}/classes/${classId}/students/upload`, {
      method: 'POST',
      headers: token ? { Authorization: `Bearer ${token}` } : {},
      body: form
    }).then(async (r) => {
      if (!r.ok) {
        const e = await r.json().catch(() => ({}));
        throw new Error(e.detail || '업로드 실패');
      }
      return r.json();
    });
  },

  // Public (students)
  getPublicClass: (code) => req(`/classes/public/${code}`),
  getActivePresentation: (code) => req(`/presentations/active/${code}`),
  getActivePresentationByClassId: (classId) =>
    req(`/presentations/class/${classId}/active`),

  // Teacher
  startPresentation: (d) =>
    req('/presentations/', { method: 'POST', body: JSON.stringify(d) }),
  endPresentation: (id) => req(`/presentations/${id}/end`, { method: 'PUT' }),
  getClassResults: (classId) => req(`/presentations/class/${classId}/results`),

  // Scores
  submitScore: (d) => req('/scores/', { method: 'POST', body: JSON.stringify(d) })
};
