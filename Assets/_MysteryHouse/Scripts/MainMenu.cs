using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.SceneManagement;

public class MainMenu : MonoBehaviour
{
    [SerializeField]
    private string sceneName = null;
    // Start is called before the first frame update
    void Start()
    {
        StartCoroutine(LoadGameCor());
    }
   
    IEnumerator LoadGameCor()
    {
        yield return new WaitForSeconds(5.0f);

        LoadGame();
    }

    public void LoadGame()
    {
        SceneManager.LoadScene(sceneName);
    }


}
