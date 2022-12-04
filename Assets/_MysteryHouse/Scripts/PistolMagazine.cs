using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PistolMagazine : MonoBehaviour
{

    [SerializeField]
    private int maxAmmo = 6;

    public int MaxAmmo => maxAmmo;

    void Start()
    {
        
    }

    public int GetMagazineAmmo()
    {
        int currentMaxAmmo = maxAmmo;
        maxAmmo = 0;
        return currentMaxAmmo;
    }

  
}
