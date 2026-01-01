# UserLAnd Best Practices Guide

This document outlines coding standards, architectural patterns, and best practices for the UserLAnd project.

## Table of Contents
1. [Architecture Patterns](#architecture-patterns)
2. [Kotlin Best Practices](#kotlin-best-practices)
3. [Android Best Practices](#android-best-practices)
4. [Testing Best Practices](#testing-best-practices)
5. [Git Workflow](#git-workflow)
6. [Performance Guidelines](#performance-guidelines)
7. [Security Guidelines](#security-guidelines)

## Architecture Patterns

### MVVM-C Architecture

UserLAnd follows the MVVM-C (Model-View-ViewModel-Coordinator) architecture:

```
UI Layer (Fragments/Activities)
    ↓ observes
ViewModel Layer
    ↓ uses
Repository/Model Layer
    ↓ uses
Data Sources (Room, Network, Files)
```

**Guidelines:**

- **Fragments/Activities**: Handle only UI logic, no business logic
- **ViewModels**: Coordinate between UI and data layers, expose LiveData/StateFlow
- **Repositories**: Abstract data sources, handle data operations
- **Models**: Plain data classes, entities, DTOs

**Example:**

```kotlin
// ✅ Good: ViewModel exposes LiveData, Fragment observes
class SessionListViewModel(private val repository: SessionRepository) : ViewModel() {
    val sessions: LiveData<List<Session>> = repository.getAllSessions()
    
    fun deleteSession(sessionId: Long) {
        viewModelScope.launch {
            repository.deleteSession(sessionId)
        }
    }
}

class SessionListFragment : Fragment() {
    private val viewModel: SessionListViewModel by viewModels()
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        viewModel.sessions.observe(viewLifecycleOwner) { sessions ->
            adapter.submitList(sessions)
        }
    }
}

// ❌ Bad: Fragment directly accesses database
class SessionListFragment : Fragment() {
    private lateinit var database: UlaDatabase
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        // DON'T DO THIS
        database.sessionDao().getAllSessions().observe(viewLifecycleOwner) { sessions ->
            adapter.submitList(sessions)
        }
    }
}
```

## Kotlin Best Practices

### Null Safety

```kotlin
// ✅ Use safe calls and Elvis operator
val length = name?.length ?: 0

// ✅ Use let for null checks
user?.let { 
    updateUI(it)
}

// ❌ Avoid !! operator unless absolutely necessary
val length = name!!.length  // Don't do this
```

### Coroutines

```kotlin
// ✅ Use viewModelScope in ViewModels
class MyViewModel : ViewModel() {
    fun loadData() {
        viewModelScope.launch {
            val data = repository.getData()
            _uiState.value = data
        }
    }
}

// ✅ Use lifecycleScope in Fragments
class MyFragment : Fragment() {
    fun performAction() {
        viewLifecycleOwner.lifecycleScope.launch {
            val result = suspendingFunction()
            updateUI(result)
        }
    }
}

// ✅ Handle errors properly
viewModelScope.launch {
    try {
        val data = repository.getData()
        _uiState.value = UiState.Success(data)
    } catch (e: Exception) {
        _uiState.value = UiState.Error(e.message)
    }
}
```

### Data Classes

```kotlin
// ✅ Use data classes for data transfer
data class Session(
    val id: Long,
    val name: String,
    val filesystemId: Long,
    val isActive: Boolean = false
)

// ✅ Use copy() for immutable updates
val updatedSession = session.copy(isActive = true)

// ✅ Destructuring
val (id, name) = session
```

### Extension Functions

```kotlin
// ✅ Use extensions for utility functions
fun Context.showToast(message: String, duration: Int = Toast.LENGTH_SHORT) {
    Toast.makeText(this, message, duration).show()
}

// Usage
context.showToast("Session started")
```

### When Expressions

```kotlin
// ✅ Use sealed classes with when for state management
sealed class UiState {
    object Loading : UiState()
    data class Success(val data: List<Session>) : UiState()
    data class Error(val message: String) : UiState()
}

// ✅ Exhaustive when
when (state) {
    is UiState.Loading -> showLoading()
    is UiState.Success -> showData(state.data)
    is UiState.Error -> showError(state.message)
    // No else needed - compiler ensures all cases handled
}
```

## Android Best Practices

### ViewBinding

```kotlin
// ✅ Use ViewBinding (already enabled in build.gradle)
class MyFragment : Fragment() {
    private var _binding: FragmentMyBinding? = null
    private val binding get() = _binding!!
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentMyBinding.inflate(inflater, container, false)
        return binding.root
    }
    
    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null  // Prevent memory leaks
    }
}
```

### Resource Management

```kotlin
// ✅ Use string resources for all user-facing text
binding.textView.text = getString(R.string.session_started)

// ✅ Use dimension resources for spacing
val padding = resources.getDimensionPixelSize(R.dimen.default_padding)

// ✅ Use color resources
binding.view.setBackgroundColor(ContextCompat.getColor(context, R.color.primary))
```

### Lifecycle Awareness

```kotlin
// ✅ Observe LiveData with viewLifecycleOwner in Fragments
viewModel.data.observe(viewLifecycleOwner) { data ->
    updateUI(data)
}

// ❌ Don't use 'this' in Fragments
// viewModel.data.observe(this) { }  // Wrong!

// ✅ Use lifecycle-aware components
class LocationObserver(private val lifecycle: Lifecycle) : DefaultLifecycleObserver {
    override fun onStart(owner: LifecycleOwner) {
        startLocationUpdates()
    }
    
    override fun onStop(owner: LifecycleOwner) {
        stopLocationUpdates()
    }
}
```

### Navigation

```kotlin
// ✅ Use Navigation Component with Safe Args
val action = SessionListFragmentDirections
    .actionSessionListToSessionEdit(sessionId)
findNavController().navigate(action)

// ✅ Handle back button properly
requireActivity().onBackPressedDispatcher.addCallback(viewLifecycleOwner) {
    // Handle back press
    if (hasUnsavedChanges) {
        showConfirmationDialog()
    } else {
        isEnabled = false
        requireActivity().onBackPressed()
    }
}
```

## Testing Best Practices

### Unit Tests

```kotlin
// ✅ Use descriptive test names with backticks
@Test
fun `createSession should insert session into database`() {
    // Arrange
    val session = Session(id = 1, name = "Test")
    
    // Act
    repository.createSession(session)
    
    // Assert
    val result = repository.getSession(1)
    assertEquals(session, result)
}

// ✅ Use test fixtures
object TestFixtures {
    fun createTestSession(
        id: Long = 1,
        name: String = "Test Session"
    ) = Session(id = id, name = name)
}

// ✅ Mock dependencies
@Test
fun `loadSessions should update UI state on success`() {
    val mockRepository = mock<SessionRepository>()
    whenever(mockRepository.getAllSessions()).thenReturn(flowOf(testSessions))
    
    val viewModel = SessionViewModel(mockRepository)
    
    assertEquals(UiState.Success(testSessions), viewModel.uiState.value)
}
```

### Instrumented Tests

```kotlin
// ✅ Use AndroidJUnit4 runner
@RunWith(AndroidJUnit4::class)
class SessionDaoTest {
    private lateinit var database: UlaDatabase
    private lateinit var sessionDao: SessionDao
    
    @Before
    fun setup() {
        database = Room.inMemoryDatabaseBuilder(
            ApplicationProvider.getApplicationContext(),
            UlaDatabase::class.java
        ).build()
        sessionDao = database.sessionDao()
    }
    
    @After
    fun teardown() {
        database.close()
    }
    
    @Test
    fun insertAndRetrieveSession() = runBlocking {
        val session = TestFixtures.createTestSession()
        sessionDao.insert(session)
        
        val retrieved = sessionDao.getSession(session.id)
        assertEquals(session, retrieved)
    }
}
```

## Git Workflow

### Branch Naming

```bash
# ✅ Good branch names
feature/add-ssh-support
fix/session-crash-on-start
docs/update-readme
chore/update-dependencies
refactor/improve-viewmodel

# ❌ Bad branch names
my-changes
fix
test
new-feature
```

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```bash
# ✅ Good commit messages
feat: Add support for custom SSH ports
fix: Resolve crash when starting session without filesystem
docs: Update installation instructions
style: Format code according to ktlint rules
refactor: Extract session startup logic to separate class
test: Add unit tests for SessionRepository
chore: Update Gradle dependencies

# ❌ Bad commit messages
Fixed stuff
WIP
Updates
asdf
Changes
```

### Pull Request Size

- **Keep PRs small**: Aim for < 400 lines of code changes
- **Single responsibility**: Each PR should address one issue/feature
- **Draft PRs**: Use draft PRs for work in progress

## Performance Guidelines

### Database Operations

```kotlin
// ✅ Use suspend functions for database operations
@Dao
interface SessionDao {
    @Query("SELECT * FROM sessions")
    suspend fun getAllSessions(): List<Session>
    
    @Insert
    suspend fun insert(session: Session)
}

// ✅ Use Flow for observing data
@Query("SELECT * FROM sessions")
fun observeSessions(): Flow<List<Session>>

// ✅ Batch operations
@Insert
suspend fun insertAll(sessions: List<Session>)
```

### Memory Management

```kotlin
// ✅ Avoid memory leaks with proper lifecycle handling
class MyFragment : Fragment() {
    private var _binding: FragmentMyBinding? = null
    private val binding get() = _binding!!
    
    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null  // Clear binding reference
    }
}

// ✅ Use weak references for callbacks
class DataLoader(listener: LoadListener) {
    private val listenerRef = WeakReference(listener)
    
    fun notifyComplete() {
        listenerRef.get()?.onComplete()
    }
}
```

### List Performance

```kotlin
// ✅ Use RecyclerView with DiffUtil
class SessionAdapter : ListAdapter<Session, SessionViewHolder>(SessionDiffCallback()) {
    // Implementation
}

class SessionDiffCallback : DiffUtil.ItemCallback<Session>() {
    override fun areItemsTheSame(oldItem: Session, newItem: Session) =
        oldItem.id == newItem.id
    
    override fun areContentsTheSame(oldItem: Session, newItem: Session) =
        oldItem == newItem
}

// ✅ Use ViewHolder pattern
class SessionViewHolder(private val binding: ItemSessionBinding) :
    RecyclerView.ViewHolder(binding.root) {
    
    fun bind(session: Session) {
        binding.sessionName.text = session.name
    }
}
```

## Security Guidelines

### Sensitive Data

```kotlin
// ✅ Never log sensitive data
Log.d(TAG, "User logged in: ${user.username}")  // OK
// Log.d(TAG, "Password: ${user.password}")  // NEVER!

// ✅ Use EncryptedSharedPreferences for sensitive data
val masterKey = MasterKey.Builder(context)
    .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
    .build()

val sharedPreferences = EncryptedSharedPreferences.create(
    context,
    "secure_prefs",
    masterKey,
    EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
    EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
)
```

### Input Validation

```kotlin
// ✅ Validate user input
fun validateSessionName(name: String): Boolean {
    return name.isNotBlank() && 
           name.length in 1..50 &&
           name.matches(Regex("[a-zA-Z0-9_-]+"))
}

// ✅ Sanitize file paths
fun sanitizePath(path: String): String {
    return path.replace("..", "")
        .replace("~", "")
        .trim()
}
```

### Permissions

```kotlin
// ✅ Check permissions before use
if (ContextCompat.checkSelfPermission(
        context,
        Manifest.permission.WRITE_EXTERNAL_STORAGE
    ) == PackageManager.PERMISSION_GRANTED
) {
    // Permission granted
    writeFile()
} else {
    // Request permission
    requestPermissionLauncher.launch(Manifest.permission.WRITE_EXTERNAL_STORAGE)
}
```

## Code Review Checklist

Before submitting a PR, ensure:

- [ ] Code follows Kotlin style guide (run `./gradlew ktlint`)
- [ ] All tests pass (`./gradlew testAll`)
- [ ] No new compiler warnings
- [ ] Documentation updated if needed
- [ ] No sensitive data in code or logs
- [ ] Memory leaks prevented (bindings cleared, weak references used)
- [ ] Proper error handling (try-catch, null checks)
- [ ] UI tested on different screen sizes
- [ ] Performance acceptable (no UI freezes)
- [ ] Accessibility considered (content descriptions added)

## Additional Resources

- [Kotlin Coding Conventions](https://kotlinlang.org/docs/coding-conventions.html)
- [Android Architecture Guide](https://developer.android.com/topic/architecture)
- [Material Design Guidelines](https://material.io/design)
- [Android Best Practices](https://developer.android.com/topic/best-practices)

## Questions?

If you're unsure about any best practice:

1. Check existing code for examples
2. Ask in PR comments
3. Discuss in GitHub Discussions
4. Reference official Android/Kotlin documentation

Remember: These are guidelines, not strict rules. Use your best judgment and prioritize code clarity and maintainability.
