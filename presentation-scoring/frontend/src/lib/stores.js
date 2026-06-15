import { writable } from 'svelte/store';
import { browser } from '$app/environment';

function createAuth() {
  const token = browser ? localStorage.getItem('token') : null;
  const name = browser ? localStorage.getItem('teacherName') : null;

  const { subscribe, set } = writable({
    token,
    name,
    isAuthenticated: !!token
  });

  return {
    subscribe,
    login(token, name) {
      if (browser) {
        localStorage.setItem('token', token);
        localStorage.setItem('teacherName', name);
      }
      set({ token, name, isAuthenticated: true });
    },
    logout() {
      if (browser) {
        localStorage.removeItem('token');
        localStorage.removeItem('teacherName');
      }
      set({ token: null, name: null, isAuthenticated: false });
    }
  };
}

export const auth = createAuth();
