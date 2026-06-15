<script>
  import { onMount } from 'svelte';
  import { auth } from '$lib/stores';
  import { api } from '$lib/api';
  import { goto } from '$app/navigation';

  let tab = 'login';
  let name = '', email = '', password = '';
  let error = '', loading = false;

  onMount(() => {
    if ($auth.isAuthenticated) goto('/teacher/dashboard');
  });

  async function submit() {
    error = '';
    loading = true;
    try {
      const res =
        tab === 'login'
          ? await api.login({ email, password })
          : await api.register({ name, email, password });
      auth.login(res.access_token, res.teacher_name);
      goto('/teacher/dashboard');
    } catch (e) {
      error = e.message;
    } finally {
      loading = false;
    }
  }

  function switchTab(t) {
    tab = t;
    error = '';
  }
</script>

<svelte:head><title>발표 평가 시스템 - 교사 로그인</title></svelte:head>

<div class="min-h-screen bg-gradient-to-br from-blue-700 to-blue-900 flex items-center justify-center p-4">
  <div class="bg-white rounded-2xl shadow-2xl w-full max-w-md p-8">
    <div class="text-center mb-8">
      <div class="text-6xl mb-3">🎤</div>
      <h1 class="text-2xl font-bold text-gray-800">발표 평가 시스템</h1>
      <p class="text-gray-400 text-sm mt-1">교사용 관리 페이지</p>
    </div>

    <!-- Tab -->
    <div class="flex mb-6 bg-gray-100 rounded-xl p-1">
      {#each [{ key: 'login', label: '로그인' }, { key: 'register', label: '회원가입' }] as t}
        <button
          class="flex-1 py-2 rounded-lg text-sm font-medium transition-all {
            tab === t.key ? 'bg-white shadow text-blue-700' : 'text-gray-500 hover:text-gray-700'
          }"
          on:click={() => switchTab(t.key)}
        >{t.label}</button>
      {/each}
    </div>

    <form on:submit|preventDefault={submit} class="space-y-4">
      {#if tab === 'register'}
        <div>
          <label class="block text-sm font-medium text-gray-700 mb-1">이름</label>
          <input type="text" bind:value={name} required placeholder="홍길동" class="input" />
        </div>
      {/if}
      <div>
        <label class="block text-sm font-medium text-gray-700 mb-1">이메일</label>
        <input type="email" bind:value={email} required placeholder="teacher@school.edu" class="input" />
      </div>
      <div>
        <label class="block text-sm font-medium text-gray-700 mb-1">비밀번호</label>
        <input type="password" bind:value={password} required placeholder="&bull;&bull;&bull;&bull;&bull;&bull;&bull;&bull;" class="input" />
      </div>

      {#if error}
        <div class="bg-red-50 border border-red-200 text-red-600 px-3 py-2.5 rounded-xl text-sm">
          {error}
        </div>
      {/if}

      <button type="submit" disabled={loading} class="btn-primary w-full mt-2">
        {loading ? '처리 중...' : tab === 'login' ? '로그인' : '회원가입'}
      </button>
    </form>
  </div>
</div>
