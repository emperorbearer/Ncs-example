<script>
  import '../app.css';
  import { auth } from '$lib/stores';
  import { goto } from '$app/navigation';
  import { page } from '$app/stores';

  $: isTeacher = $page.url.pathname.startsWith('/teacher');

  function logout() {
    auth.logout();
    goto('/');
  }
</script>

{#if isTeacher && $auth.isAuthenticated}
  <nav class="bg-blue-700 text-white shadow-lg">
    <div class="max-w-5xl mx-auto px-4 py-3 flex justify-between items-center">
      <a href="/teacher/dashboard" class="font-bold text-lg flex items-center gap-2">
        <span class="text-xl">🎤</span> 발표 평가 시스템
      </a>
      <div class="flex items-center gap-4">
        <span class="text-blue-200 text-sm">{$auth.name} 선생님</span>
        <button
          on:click={logout}
          class="bg-blue-800 hover:bg-blue-900 text-white text-sm px-3 py-1.5 rounded-lg transition-colors"
        >
          로그아웃
        </button>
      </div>
    </div>
  </nav>
{/if}

<slot />
